#!/usr/bin/env python3
"""Records/recovery via public PC commands and observations in separate processes.

20 m inputs use independent gauge pressures 1.96133/2.01036325 bar.
Record peak is the existing decimetre sensor display, expected exactly 200 dm.
External legacy/corrupt file construction tests compatibility without rewriting.
"""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib

exe = sys.argv[1]
def run(state, mode):
    subprocess.run([exe, str(state), mode], check=True)

with tempfile.TemporaryDirectory(prefix='water-records-') as temporary:
    root = Path(temporary)
    state = root/'records'
    run(state, 'record')
    original = {p.name: p.read_bytes() for p in state.glob('dive-*.log')}
    preference = (state/'water-preferences.txt').read_bytes()
    run(state, 'inspect')
    assert {p.name: p.read_bytes() for p in state.glob('dive-*.log')} == original
    assert (state/'water-preferences.txt').read_bytes() == preference
    legacy = [b'DIVE_WATCH_LOG 1 3 40 90 20 30 70 0 preserved-old-policy\n',
              b'DIVE_WATCH_LOG 2 4 444 40 90 20 30 70 1 preserved-old-policy\n']
    for index, content in enumerate(legacy, 3):
        (state/f'dive-{index:020}.log').write_bytes(content)
    run(state, 'legacy')
    for index, content in enumerate(legacy, 3):
        assert (state/f'dive-{index:020}.log').read_bytes() == content
    cross = root/'cross'
    run(cross, 'save')
    run(cross, 'cross-records')
    memory = root/'memory'
    run(memory, 'save')
    run(memory, 'memory')
    recovery = root/'recovery'
    run(recovery, 'save')
    checkpoint = recovery/'recovery.checkpoint'
    saved = checkpoint.read_bytes()
    for mode in ('gap', 'clock'):
        run(recovery, mode)
        assert checkpoint.read_bytes() == saved
    old = bytearray(saved)
    struct.pack_into('<I', old, 4, 2)
    old[56:64] = bytes(8)  # historical active PC schema2: water/depth lifecycle ambiguous
    struct.pack_into('<I', old, len(old)-4, zlib.crc32(old[:-4]))
    checkpoint.write_bytes(old)
    run(recovery, 'incompatible')
    assert checkpoint.read_bytes() == old
    checkpoint.write_bytes(saved)
    run(recovery, 'resume')
    assert checkpoint.read_bytes() == saved
    assert (recovery/'water-preferences.txt').read_bytes() == b'DIVE_WATCH_WATER 1 1\n'
print('WATER RECORDS PASS: separate processes, original 20m peaks and water retained, legacy unknown/immutable, active Fresh recovery versus next Marine, memory/disk locks, gap/time/schema rejection stays INVALID')
