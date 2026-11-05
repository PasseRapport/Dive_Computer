#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Accept the public build/report artifacts against the migration specification.

Run after build_target.py; no probe, simulator or production helper is used.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
args = parser.parse_args()
build = args.build_dir.resolve()
report_path = build / 'acceptance-report.json'
subprocess.run([sys.executable, str(Path(__file__).resolve().parents[2] /
                'tools/report_target.py'), str(build), '--output', str(report_path)], check=True)
report = json.loads(report_path.read_text())
assert report['board'] == 'dc_v2_2/nrf52840', report['board']
assert report['device'] == {'soc': 'nrf52840', 'package': 'QIAA',
                            'total_flash_bytes': 1048576, 'total_ram_bytes': 262144}
assert report['elf_sha256'] == hashlib.sha256((build / 'zephyr/zephyr.elf').read_bytes()).hexdigest()
assert report['regions']['FLASH']['origin'] == 49152
assert report['regions']['FLASH']['capacity_bytes'] == 487424
assert report['regions']['RAM']['origin'] == 536870912
assert report['regions']['RAM']['capacity_bytes'] == 262144
for region in report['regions'].values():
    assert 0 < region['used_bytes'] < region['capacity_bytes']
    assert region['remaining_bytes'] == region['capacity_bytes'] - region['used_bytes']
assert report['target_execution'] is False
assert report['full_watch_fit_established'] is False
assert len(report['missing_application_features']) == 4
assert report['regout0_policy']['intended_microvolt'] == 1800000
assert report['regout0_policy']['intended_uicr_vout'] == 0
assert report['regout0_policy']['hex_contains_regout0'] is False
assert report['regout0_policy']['hex_regout0_value'] is None
assert report['regout0_policy']['effective_voltage_established'] is False
print('PASS: replacement target, independent capacities, image link region and truthful report')
