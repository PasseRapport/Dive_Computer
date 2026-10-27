# SPDX-License-Identifier: GPL-3.0-or-later
"""Public replay fault/availability contract; no numerical schedule oracle."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli = sys.argv[1]
with tempfile.TemporaryDirectory() as tmp:
    profile = Path(tmp) / 'profile.csv'
    start = 'timestamp_seconds,absolute_pressure_bar\n0,1.01325\n120,2.01325\n'
    for bad in ('120,2.01325\n', '119,2.01325\n', '3721,2.01325\n',
                '130,nan\n', '130,inf\n', '130,0.2\n', '130,99\n',
                '4294967296,2\n', '130,2 extra\n', 'missing\n'):
        profile.write_text(start + bad)
        process = subprocess.run([cli, str(profile)], text=True, capture_output=True)
        assert process.returncode != 0, bad
        rows = [json.loads(line) for line in process.stdout.splitlines()]
        assert rows[0]['exposure_policy'] == 'ostc4-continuous-air-gf30-70-v1'
        assert rows[0]['correction_set'] == 'cached-10s-and-stable-schreiner-v1'
        assert rows[0]['initialization'] == 'upstream-surface-equilibrium'
        assert rows[-1]['timestamp_seconds'] == 120
        assert set(rows[-1]['availability'].values()) == {'invalid'}, bad
    # A valid zero is different from the unavailable initial result.
    profile.write_text(start)
    rows = [json.loads(line) for line in subprocess.check_output([cli, str(profile)], text=True).splitlines()]
    assert rows[1]['tts_seconds'] == 0 and rows[1]['availability']['tts'] == 'unavailable'
    assert rows[2]['tts_seconds'] == 0 and rows[2]['availability']['tts'] == 'available'
    assert rows[0]['no_deco_tts_semantics'] == 'upstream-zero-not-transit-time'
print('continuity replay faults and explicit availability pass')
