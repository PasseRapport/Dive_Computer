#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the generated sensor/charger declaration; no probe or sensor access."""
import argparse
from pathlib import Path
import json
import re
import sys
from artifact_dt import cells, enabled

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--ncs', type=Path, default=Path.home() / 'ncs/v3.4.1')
args = parser.parse_args()
sys.path.insert(0, str(args.ncs / 'zephyr/scripts/dts/python-devicetree/src'))
from devicetree import dtlib

dt = dtlib.DT(str(args.build_dir / 'zephyr/zephyr.dts'))
bus = dt.label2node['i2c0']
# Independent addresses transcribed from the approved wiring contract.
expected = {'st,lis2mdl': 30, 'st,lis2dux12': 25, 'meas,ms5837-30ba': 118,
            'ti,tmag3001': 52, 'vishay,veml6035': 16, 'ti,drv2624': 90}
actual = {n.props['compatible'].to_string(): n.props['reg'].to_num()
          for n in bus.nodes.values()}
assert actual == expected, (actual, expected)
for node in bus.nodes.values():
    if node.props['compatible'].to_string() in ('st,lis2mdl', 'ti,drv2624'):
        assert not any(name.endswith('-gpios') for name in node.props), node.path
print('PASS: generated six-device I2C address contract, including grounded-ADDR Hall at 0x34')

config = dict(re.findall(r'^(CONFIG_\w+)=(.*)$',
                        (args.build_dir / 'zephyr/.config').read_text(), re.M))
assert config['CONFIG_I2C'] == config['CONFIG_PINCTRL'] == 'y'
assert config['CONFIG_I2C_NRFX_TWIM'] == 'y'
assert config.get('CONFIG_SENSOR', 'n') == 'n'
assert bus.props['clock-frequency'].to_num() == 400000
assert bus.props['status'].to_string() == 'okay'
# PSEL function IDs and literal pins are independent expectations from Nordic
# pinctrl's public encoding: function in bits 24..31, GPIO index in bits 0..8.
for state in ['i2c0_default', 'i2c0_sleep']:
    group = dt.label2node[state].nodes['group1']
    assert group.props['psels'].to_nums() == [184549391, 201326609]
    assert group.props['nordic,drive-mode'].to_num() == 6  # S0D1, never drive high
    assert 'bias-pull-up' not in group.props
    if state == 'i2c0_sleep':
        assert 'low-power-enable' in group.props
commands = json.loads((args.build_dir / 'compile_commands.json').read_text())
compiled = {Path(c['file']).name for c in commands}
assert {'pinctrl_nrf.c', 'i2c_nrfx_twim.c'} <= compiled
print('PASS: compiled TWIM0/pinctrl, 400 kHz and explicit open-drain P0.15/P0.17')

gpio0 = dt.label2node['gpio0'].props['phandle'].to_num()


# Sensor properties describe MCU inputs: active high ACC, active low Hall/light;
# external interrupt pull-ups belong to the switched peripheral rail.
for label, prop, pin, flags in [('acc', 'int1-gpios', 2, 0),
                                ('enc', 'int-gpios', 3, 1),
                                ('light', 'int-gpios', 30, 1)]:
    assert cells(dt.label2node[label].props[prop]) == [gpio0, pin, flags]
user = dt.get_node('/zephyr,user')
assert cells(user.props['chg_n-gpios']) == [gpio0, 10, 17]  # LOW | PULL_UP
assert 'chg_n-input' in user.props
assert user.props['chg_n-pullup-domain'].to_string() == 'mcu-vdd'
assert 'nfct-pins-as-gpios' in dt.label2node['uicr'].props

# Check every enabled GPIO declaration for these signals, including declarations
# outside the sensor nodes; duplicated keys/interrupt consumers must fail.
owners = {}


for node in dt.node_iter():
    if not enabled(node):
        continue
    for name, prop in node.props.items():
        if name == 'gpios' or name.endswith('-gpios'):
            values = cells(prop)
            for offset in range(0, len(values), 3):
                port, pin, flags = values[offset:offset + 3]
                if port == gpio0 and pin in (2, 3, 9, 10, 28, 30):
                    owners.setdefault(pin, []).append(f'{node.path}:{name}')
assert set(owners) == {2, 3, 9, 10, 28, 30}, owners
assert all(len(v) == 1 for v in owners.values()), owners
print('PASS: sensor interrupt polarities, charger input/bias policy, NFC and unique GPIO owners')
assert dt.root.props['compatible'].to_string() == 'dive,dc-v2-2'
