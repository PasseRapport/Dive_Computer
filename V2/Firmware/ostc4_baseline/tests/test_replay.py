# SPDX-License-Identifier: GPL-3.0-or-later
"""Public replay contracts, not independent numerical schedule expectations."""
import json
import pathlib
import subprocess
import sys
import tempfile

cli, root = sys.argv[1], pathlib.Path(sys.argv[2])
for name in ('shallow', 'decompression'):
    fixture = root / 'fixtures' / (name + '.csv')
    first = subprocess.check_output([cli, str(fixture)])
    assert first == subprocess.check_output([cli, str(fixture)])
    rows = [json.loads(line) for line in first.splitlines()]
    assert rows[0]['revision'] == '7198d203520f3a3cdccf1ccc0bf8af6475ee4819'
    assert rows[0]['synthetic'] is True
    assert rows[1]['availability']['ndl'] == 'unavailable'
    assert all(len(r['nitrogen_bar']) == 16 for r in rows[1:])
    assert all(len(r['stop_seconds']) == 50 for r in rows[1:])
    if name == 'shallow':
        assert any(r['ndl_seconds'] > 0 and r['tts_seconds'] == 0 and
                   r['availability']['tts'] == 'available' for r in rows[1:])
    else:
        assert any(r['tts_seconds'] > 0 and sum(r['stop_seconds']) > 0 for r in rows[1:])

bad_rows = ['30,2\n30,2\n', '1,nan\n', '1,inf\n', '1,0.1\n',
            '1,99\n', '0,1.01325\n', '1.5,2\n', '-1,2\n',
            '4294967296,2\n', '1,2 extra\n', '3601,2\n']
with tempfile.TemporaryDirectory() as tmp:
    profile = pathlib.Path(tmp) / 'bad.csv'
    for row in bad_rows:
        profile.write_text('timestamp_seconds,absolute_pressure_bar\n0,1.01325\n' + row)
        result = subprocess.run([cli, str(profile)], capture_output=True)
        assert result.returncode != 0, row
    profile.write_text('timestamp_seconds,absolute_pressure_bar\n')
    assert subprocess.run([cli, str(profile)], capture_output=True).returncode != 0
print('deterministic synthetic replay and malformed-profile contracts pass')
