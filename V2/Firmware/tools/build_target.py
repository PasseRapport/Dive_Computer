#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Build the custom board with the installed NCS; never flash or execute it."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', required=True, type=Path)
parser.add_argument('--ncs', type=Path, default=Path.home() / 'ncs/v3.4.1')
parser.add_argument('--toolchain', type=Path, default=Path.home() / 'ncs/toolchains/8285d8ad56')
args = parser.parse_args()
source = Path(__file__).resolve().parents[1]
env = os.environ.copy()
for entry in json.loads((args.toolchain / 'environment.json').read_text())['env_vars']:
    value = (os.pathsep.join(str(args.toolchain / p) for p in entry['values'])
             if entry['type'] == 'relative_paths' else entry['value'])
    key = entry['key']
    if entry.get('existing_value_treatment') == 'prepend_to' and env.get(key):
        value += os.pathsep + env[key]
    env[key] = value
env.update(ZEPHYR_BASE=str(args.ncs / 'zephyr'),
           ZEPHYR_TOOLCHAIN_VARIANT='zephyr',
           ZEPHYR_SDK_INSTALL_DIR=str(args.toolchain / 'opt/zephyr-sdk'),
           CCACHE_DISABLE='1', XDG_CACHE_HOME=str(args.build_dir.resolve() / '.cache'))
# Use host tools with their own system libraries; retain bundled Python's lib.
env['LD_LIBRARY_PATH'] = str(args.toolchain / 'usr/local/lib')
subprocess.run([str(args.toolchain / 'usr/local/bin/west'), 'build', '--no-sysbuild',
                '-b', 'dc_v2_1/nrf52833', '-d', str(args.build_dir.resolve()),
                str(source), '--', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
                '-DUSER_CACHE_DIR=' + str(args.build_dir.resolve() / '.cache')],
               env=env, check=True, cwd=args.ncs)
