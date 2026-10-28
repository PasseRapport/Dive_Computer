#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Report the complete configured ELF, without implying execution or full-watch fit."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build_dir', type=Path)
parser.add_argument('--output', required=True, type=Path)
parser.add_argument('--toolchain', type=Path, default=Path.home() / 'ncs/toolchains/8285d8ad56')
args = parser.parse_args()
zephyr = args.build_dir / 'zephyr'
elf = zephyr / 'zephyr.elf'
tools = args.toolchain / 'opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin'

def run(tool, *arguments):
    return subprocess.check_output([str(tools / ('arm-zephyr-eabi-' + tool)),
                                    *map(str, arguments)], text=True)

symbols = {}
sizes = {}
types = {}
for line in run('nm', '-S', elf).splitlines():
    fields = line.split()
    if len(fields) == 4:
        address, size, kind, name = fields
        types[name] = kind
        symbols[name], sizes[name] = int(address, 16), int(size, 16)
    elif len(fields) == 3 and fields[1].upper() != 'U':
        symbols[fields[2]] = int(fields[0], 16)
regions = {}
for line in (zephyr / 'zephyr.map').read_text().splitlines():
    match = re.fullmatch(r'(FLASH|RAM)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+\w+', line)
    if match:
        regions[match[1]] = {'origin': int(match[2], 16), 'capacity_bytes': int(match[3], 16)}
for name, used in [('FLASH', symbols['_flash_used']), ('RAM', symbols['_image_ram_size'])]:
    regions[name].update(used_bytes=used,
                         remaining_bytes=regions[name]['capacity_bytes'] - used)
config_text = (zephyr / '.config').read_text()
config = dict(re.findall(r'^(CONFIG_\w+)=(.*)$', config_text, re.M))
keys = ['MAIN_STACK_SIZE', 'ISR_STACK_SIZE', 'IDLE_STACK_SIZE', 'SYSTEM_WORKQUEUE_STACK_SIZE',
        'HEAP_MEM_POOL_SIZE', 'COMMON_LIBC_MALLOC_ARENA_SIZE', 'FPU', 'FP_HARDABI',
        'FP_SOFTABI', 'PICOLIBC', 'USE_DT_CODE_PARTITION', 'RTT_CONSOLE', 'USE_SEGGER_RTT',
        'BT', 'DISPLAY', 'LVGL', 'SENSOR', 'I2C', 'SPI']
commands = json.loads((args.build_dir / 'compile_commands.json').read_text())
core_commands = [c for c in commands if c['file'].endswith(
    ('/ostc4_baseline.c', '/corrected/decom.c', '/corrected/buehlmann-contained.c'))]
map_text = (zephyr / 'zephyr.map').read_text()
math_members = sorted(set(re.findall(r'\S+libc\.a\(libm_[^)]+\)', map_text)))
source = Path(__file__).resolve().parents[1] / 'ostc4_baseline'
report = {
    'evidence_level': 'custom-board compilation and static linked-image analysis only',
    'full_watch_fit_established': False,
    'target_execution': False,
    'board': 'dc_v2_1/nrf52833',
    'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(),
    'compiler': run('gcc', '--version').splitlines()[0],
    'regions': regions,
    'allocated_stack_objects_bytes': {name: size for name, size in sizes.items()
                                     if 'stack' in name and types[name].lower() == 'b'},
    'configuration': {key: config.get('CONFIG_' + key, 'n') for key in keys},
    'ram_note': 'Remaining is linker headroom; no runtime stack/heap/timing measurement.',
    'missing_application_features': ['JDI display driver and target LVGL presentation',
        'Bluetooth application/services', 'sensor acquisition and complete pressure history',
        'persistent clock/storage/restart adapter'],
    'elf_attributes': run('readelf', '-A', elf),
    'linked_math_symbols': sorted(name for name in symbols if name in
        ['exp', 'expf', 'expm1f', 'log', 'logf', 'pow', 'powf', 'sqrt', 'sqrtf', 'ceil', 'ceilf']
        or name.startswith('__aeabi_d')),
    'math_archive_members': math_members,
    'math_archive_note': 'Map archive members can contain discarded sections; linked_math_symbols lists retained named math functions.',
    'core_compile_commands': core_commands,
    'generated_source_sha256': {name: hashlib.sha256((args.build_dir / 'corrected' / name).read_bytes()).hexdigest()
                               for name in ['decom.c', 'buehlmann-contained.c']},
    'policy': json.loads((source / 'corrected-policy.json').read_text()),
}
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'regions': regions, 'allocated_stacks': report['allocated_stack_objects_bytes']}, indent=2))
