#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Accept generated display routing/controller variants; never operate hardware."""
import argparse
from pathlib import Path
import re
import sys
from artifact_dt import cells, enabled

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--variant', choices=['normal', 'spi8', 'spi4', 'pwm', 'spi4-pwm'], default='normal')
parser.add_argument('--ncs', type=Path, default=Path.home() / 'ncs/v3.4.1')
args = parser.parse_args()
spi4 = args.variant in ('spi4', 'spi4-pwm')
pwm_enabled = args.variant in ('pwm', 'spi4-pwm')
sys.path.insert(0, str(args.ncs / 'zephyr/scripts/dts/python-devicetree/src'))
from devicetree import dtlib

z = args.build_dir / 'zephyr'
dt = dtlib.DT(str(z / 'zephyr.dts'))
config = dict(re.findall(r'^(CONFIG_\w+)=(.*)$', (z / '.config').read_text(), re.M))
spi, lcd, pwm = (dt.label2node[label] for label in ['spi0', 'lcd', 'pwm0'])
def gpio(node, prop, controller, pin, flags):
    assert cells(node.props[prop]) == [dt.label2node[controller].props['phandle'].to_num(), pin, flags], prop

# Independent routing/period/mode expectations transcribed from the specification.
gpio(spi, 'cs-gpios', 'gpio0', 8, 33)
gpio(lcd, 'reset-gpios', 'gpio0', 6, 17)
gpio(lcd, 'te-gpios', 'gpio1', 9, 0)
assert 'spi-cpol' not in lcd.props and 'spi-cpha' not in lcd.props, 'mode 0'
assert lcd.props['spi-max-frequency'].to_num() == (4000000 if spi4 else 8000000)
spi_group = spi.props['pinctrl-0'].to_nodes()[0].nodes['group1']
assert spi_group.props['psels'].to_nums() == [67108868, 83886106], 'SCK P0.04, MOSI P0.26, no MISO'
drive = spi_group.props.get('nordic,drive-mode')
assert (drive.to_num() if drive else 0) == (3 if spi4 else 0)
assert 'pinctrl-0' in pwm.props, 'fallback PWM0 P0.12 routing is not prepared'
assert pwm.props['pinctrl-0'].to_nodes()[0].nodes['group1'].props['psels'].to_nums() == [369098764]
assert enabled(pwm) == (pwm_enabled), 'PWM0 disabled in normal assembly'
if pwm_enabled:
    pwm_cells = cells(dt.get_node('/zephyr,user').props['pwms'])
    assert pwm_cells == [pwm.props['phandle'].to_num(), 0, 4000000, 0], 'PWM0 ch0, 4 ms, active high'
else:
    assert 'pwms' not in dt.get_node('/zephyr,user').props

# No enabled GPIO property may claim P0.12, or duplicate display control pins.
claims = {('gpio0', 8): 0, ('gpio0', 6): 0, ('gpio1', 9): 0, ('gpio0', 12): 0,
          ('gpio0', 4): 0, ('gpio0', 26): 0}
pin_owners = {4: [], 26: [], 12: []}
for node in dt.node_iter():
    if not enabled(node):
        continue
    if 'pinctrl-0' in node.props:
        for state in node.props['pinctrl-0'].to_nodes():
            for group in state.nodes.values():
                for psel in group.props['psels'].to_nums():
                    pin = psel & 511
                    if pin in pin_owners:
                        pin_owners[pin].append(node.path)
    for name, prop in node.props.items():
        if name == 'gpios' or name.endswith('-gpios'):
            values = cells(prop)
            for offset in range(0, len(values), 3):
                for port, pin in claims:
                    phandle = dt.label2node[port].props['phandle'].to_num()
                    if values[offset:offset + 2] == [phandle, pin]:
                        claims[(port, pin)] += 1
assert list(claims.values()) == [1, 1, 1, 0, 0, 0], claims
assert pin_owners == {4: [spi.path], 26: [spi.path],
                      12: [pwm.path] if pwm_enabled else []}, pin_owners
assert 'nfct-pins-as-gpios' in dt.label2node['uicr'].props
assert 'gpio-as-nreset' in dt.label2node['uicr'].props
assert config.get('CONFIG_NRF_TRACE_PORT', 'n') == 'n'
assert config['CONFIG_USE_SEGGER_RTT'] == config['CONFIG_RTT_CONSOLE'] == 'y'
if args.variant != 'normal':
    assert config['CONFIG_SPI'] == config['CONFIG_PINCTRL'] == config['CONFIG_SPI_NRFX_SPIM'] == 'y'
    link_map = (z / 'zephyr.map').read_text()
    assert 'spi_nrfx_spim.c.obj' in link_map
    assert 'pinctrl_nrf.c.obj' in link_map
    assert re.search(r'0x[0-9a-f]+\s+spi_nrfx_spim_common_init$', link_map, re.M)
    assert re.search(r'0x[0-9a-f]+\s+pinctrl_configure_pins$', link_map, re.M)
    if pwm_enabled:
        assert config['CONFIG_PWM'] == config['CONFIG_PWM_NRFX'] == 'y'
        assert 'pwm_nrfx.c.obj' in link_map
        assert re.search(r'\.text\.pwm_nrfx_init0\s+0x0*[1-9a-f][0-9a-f]*', link_map)
else:
    assert config.get('CONFIG_PWM', 'n') == 'n'
coverage = 'compiled controllers' if args.variant != 'normal' else 'disabled fallback'
print(f'PASS: {args.variant} generated display pins/mode/drive, single ownership, {coverage}')
print('PENDING: native nRF Connect Pin Overview and physical timing/DC/power/TE/PWM qualification')
