#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check generated board/power/partition/runner contracts, without hardware access."""
import argparse
from pathlib import Path
import re
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--ncs', type=Path, default=Path.home() / 'ncs/v3.4.1')
args = parser.parse_args()
sys.path.insert(0, str(args.ncs / 'zephyr/scripts/dts/python-devicetree/src'))
from devicetree import dtlib

zephyr = args.build_dir / 'zephyr'
dt = dtlib.DT(str(zephyr / 'zephyr.dts'))
config = dict(re.findall(r'^(CONFIG_\w+)=(.*)$', (zephyr / '.config').read_text(), re.M))
assert config['CONFIG_SOC_NRF52840_QIAA'] == 'y'
assert dt.label2node['flash0'].props['reg'].to_nums() == [0, 1048576]
assert dt.label2node['sram0'].props['reg'].to_nums() == [536870912, 262144]
assert 'nfct-pins-as-gpios' in dt.label2node['uicr'].props
assert 'gpio-as-nreset' in dt.label2node['uicr'].props
assert config.get('CONFIG_NRF_TRACE_PORT', 'n') == 'n'
assert config['CONFIG_USE_SEGGER_RTT'] == config['CONFIG_RTT_CONSOLE'] == 'y'
assert dt.label2node['reg0'].props['status'].to_string() == 'okay'
assert dt.label2node['reg1'].props['regulator-initial-mode'].to_num() == 1
user = dt.get_node('/zephyr,user')
assert user.props['mcu-regout0-microvolt'].to_num() == 1800000
assert user.props['mcu-regout0-uicr-vout'].to_num() == 0
assert config['CONFIG_CLOCK_CONTROL_NRF_K32SRC_XTAL'] == 'y'
assert config['CONFIG_CLOCK_CONTROL_NRF_K32SRC_50PPM'] == 'y'
assert config['CONFIG_CLOCK_CONTROL_NRF_K32SRC_FREQUENCY'] == '32768'
assert config['CONFIG_CLOCK_CONTROL_NRF_ACCURACY'] == '50'

# Decimal literals are independently transcribed from the specification.
expected = [('boot_partition', 0, 49152), ('slot0_partition', 49152, 487424),
            ('slot1_partition', 536576, 487424), ('storage_partition', 1024000, 24576)]
endpoint = 0
for label, start, size in expected:
    assert dt.label2node[label].props['reg'].to_nums() == [start, size], label
    assert start == endpoint and start % 4096 == size % 4096 == 0
    endpoint = start + size
assert endpoint == 1048576
assert dt.get_node('/chosen').props['zephyr,code-partition'].to_path() is dt.label2node['slot0_partition']
runners = (zephyr / 'runners.yaml').read_text()
assert '--device=nRF52840_xxAA' in runners
assert '--target=nrf52840' in runners
assert 'nrf52833' not in runners.lower()
assert config['CONFIG_FPU'] == config['CONFIG_FP_HARDABI'] == config['CONFIG_PICOLIBC'] == 'y'
assert config['CONFIG_MAIN_STACK_SIZE'] == '8192'
assert config['CONFIG_COMMON_LIBC_MALLOC_ARENA_SIZE'] == '0'
print('PASS: independent generated SoC, special pins, power/clocks, partitions, runners and preserved ABI')
