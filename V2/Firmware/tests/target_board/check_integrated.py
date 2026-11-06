#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check public generated artifacts against independent physical-pad research.

No hardware access. The reference was transcribed from Nordic and independently
reconciled with KiCad, not extracted from the production board declarations.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import struct
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--variant', choices=['normal', 'combined8', 'combined4', 'combined4-pwm'], required=True)
parser.add_argument('--ncs', type=Path, default=Path.home() / 'ncs/v3.4.1')
parser.add_argument('--connection-reference', type=Path)
args = parser.parse_args()
fw = Path(__file__).resolve().parents[2]
reference = json.loads((fw / 'evidence/migration-nrf52840/ticket04/independent-pad-reference.json').read_text())
rows = reference['pads']
assert len(rows) == len({r['pad'] for r in rows}) == 74
assert Counter(r['category'] for r in rows) == {'application': 16, 'spare_gpio': 30, 'lf_crystal': 2, 'fixed': 25, 'exposed_ground': 1}
assert all(r.get('intended') for r in rows)
application = {r['intended']['signal']: r for r in rows if r['category'] == 'application'}
assert len(application) == 16
if args.connection_reference:
    text = args.connection_reference.read_text()
    # Complete physical table rows use pad + manufacturer identity in first cells.
    table = re.findall(r'^\| ([A-Z]+\d+|EP) \| ([^|]+) \|', text, re.M)
    table = [(pad, name.strip()) for pad, name in table]
    expected = [(r['pad'], r['name']) for r in rows]
    assert Counter(table) == Counter(expected), 'delivery physical table omitted/duplicated/renamed a pad'

sys.path.insert(0, str(args.ncs / 'zephyr/scripts/dts/python-devicetree/src'))
from devicetree import dtlib
z = args.build_dir / 'zephyr'
dt = dtlib.DT(str(z / 'zephyr.dts'))
config = dict(re.findall(r'^(CONFIG_\w+)=(.*)$', (z / '.config').read_text(), re.M))

def cells(prop):
    return list(struct.unpack('>' + 'I' * (len(prop.value) // 4), prop.value))

def enabled(node):
    while node is not None:
        if 'status' in node.props and node.props['status'].to_string() != 'okay':
            return False
        node = node.parent
    return True

ports = {dt.label2node[f'gpio{n}'].props['phandle'].to_num(): n for n in [0, 1]}
owners = defaultdict(list)
serial_instances = defaultdict(list)
for node in dt.node_iter():
    if not enabled(node):
        continue
    for name, prop in node.props.items():
        if name == 'gpios' or name.endswith('-gpios'):
            values = cells(prop)
            assert len(values) % 3 == 0, (node.path, name)
            for start in range(0, len(values), 3):
                controller, pin, flags = values[start:start + 3]
                assert controller in ports, (node.path, name, controller)
                owners[ports[controller] * 32 + pin].append(f'{node.path}:{name}')
    if 'pinctrl-0' in node.props:
        for state in node.props['pinctrl-0'].to_nodes():
            for group in state.nodes.values():
                for value in group.props['psels'].to_nums():
                    owners[value & 511].append(f'{node.path}:pinctrl-0')
    if 'compatible' in node.props and 'reg' in node.props:
        compatibles = node.props['compatible'].to_strings()
        if any(re.fullmatch(r'nordic,nrf-(?:spi.*|twi.*|uart.*)', c) for c in compatibles):
            serial_instances[node.props['reg'].to_nums()[0]].append(node.path)
assert all(len(paths) == 1 for paths in owners.values()), dict(owners)
assert all(len(paths) == 1 for paths in serial_instances.values()), dict(serial_instances)
# Independent functional expectations use public DT node properties, not board helpers.
checks = {'INT_ACC1': ('acc', 'int1-gpios'), 'INT_ENC': ('enc', 'int-gpios'),
          'INT_LIGHT': ('light', 'int-gpios'), 'SCREEN_CS': ('spi1', 'cs-gpios'),
          'SCREEN_RST': ('lcd', 'reset-gpios'), 'SYNC/TE': ('lcd', 'te-gpios')}
for signal, (label, prop) in checks.items():
    values = cells(dt.label2node[label].props[prop])
    actual = ports[values[0]] * 32 + values[1]
    gpio = application[signal]['gpio']
    assert actual == int(gpio[1]) * 32 + int(gpio[3:]), signal
for signal, prop in [('~CHG', 'chg_n-gpios')]:
    value = cells(dt.get_node('/zephyr,user').props[prop])
    assert ports[value[0]] * 32 + value[1] == 10
# The independent Nordic pad identity must agree with the approved 16-signal GPIO contract.
expected = {'INT_ACC1': ('P0.02', 'A12'), 'INT_ENC': ('P0.03', 'B13'),
 'SCREEN_SCL': ('P0.04', 'J1'), 'SCREEN_SDI': ('P0.05', 'K2'),
 'QI_EN': ('P0.09', 'L24'), '~CHG': ('P0.10', 'J24'),
 'SCREEN_CS': ('P0.11', 'T2'), 'SCL': ('P0.15', 'AD10'),
 'SDA': ('P0.17', 'AD12'), 'nRESET': ('P0.18', 'AC13'),
 'BL_PWM_MCU': ('P0.20', 'AD16'), '1.2+1.8V_EN': ('P0.28', 'B11'),
 '3.1V_EN': ('P0.29', 'A10'), 'INT_LIGHT': ('P0.30', 'B9'),
 'SYNC/TE': ('P0.31', 'A8'), 'SCREEN_RST': ('P1.09', 'R1')}
assert {s: (r['gpio'], r['pad']) for s, r in application.items()} == expected
# Unique owners for every live application GPIO, plus special-function reset and
# reserved optional PWM. This spans GPIO consumers and peripheral pinctrl together.
expected_pins = {2, 3, 4, 5, 9, 10, 11, 15, 17, 28, 29, 30, 31, 41}
if args.variant == 'combined4-pwm':
    expected_pins.add(20)
assert set(owners) == expected_pins, dict(owners)
for pin, suffix in [(9, '/gpio-leds/qi_en:gpios'), (28, '/gpio-leds/rail_1v2_1v8_en:gpios'), (29, '/gpio-leds/rail_3v1_en:gpios')]:
    assert owners[pin] == [suffix]
assert 'gpio-as-nreset' in dt.label2node['uicr'].props
for label, expected_psels in [('spi1_default', [67108868, 83886085]), ('i2c0_default', [184549391, 201326609]), ('pwm0_default', [369098772])]:
    assert dt.label2node[label].nodes['group1'].props['psels'].to_nums() == expected_psels
assert config['CONFIG_GPIO'] == config['CONFIG_GPIO_NRFX'] == config['CONFIG_NRFX_GPIOTE'] == 'y'
assert enabled(dt.label2node['gpiote'])
link_map = (z / 'zephyr.map').read_text()
assert re.search(r'\.text\.gpio_nrfx_init\s+0x0*[1-9a-f][0-9a-f]*', link_map)
if args.variant != 'normal':
    assert config['CONFIG_I2C'] == config['CONFIG_SPI'] == config['CONFIG_PINCTRL'] == 'y'
    for symbol in ['i2c_nrfx_twim_common_init', 'spi_nrfx_spim_common_init', 'pinctrl_configure_pins']:
        assert re.search(r'0x[0-9a-f]+\s+' + symbol + r'$', link_map, re.M), symbol
if args.variant == 'combined4-pwm':
    assert cells(dt.get_node('/zephyr,user').props['pwms'])[1:] == [0, 4000000, 0]
print('PASS: all 16 independent GPIO/pad contracts; 74 unique physical pads; no cross-subsystem GPIO/pinctrl or shared-instance conflict')
print('PASS: applicable linked controller coverage and proposed PWM period; physical hardware and native editor inspection remain pending')
