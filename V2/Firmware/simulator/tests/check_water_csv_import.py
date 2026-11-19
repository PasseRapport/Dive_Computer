#!/usr/bin/env python3
"""Real public CSV importer + restored water, separate processes and paired frames.
Independent spec example: gauge1.96133bar =>20m Fresh/19.51219512m Marine.
Binary32 input pressure tolerance1e-6bar; physical depth tolerance1e-4m.
"""
import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from PIL import Image

binary = Path(sys.argv[1]).resolve()
evidence = Path(sys.argv[2]).resolve()
evidence.mkdir(parents=True, exist_ok=True)
source = evidence / 'pressure-input.csv'
source.write_bytes(b'timestamp_seconds,absolute_pressure_bar\n0,0.9\n2,2.86133\n10,2.86133\n')
original = source.read_bytes()
digest = hashlib.sha256(original).hexdigest()
states, accepted = [], []
for salt, density, depth, watch_dm in [(0,1000,20.0,200),(1,1025,19.51219512,195)]:
    prefix = evidence / ('marine' if salt else 'fresh')
    with tempfile.TemporaryDirectory(prefix='water-csv-state-') as tmp:
        directory = Path(tmp)
        preference = f'DIVE_WATCH_WATER 1 {salt}\n'.encode()
        path = directory/'water-preferences.txt'
        path.write_bytes(preference)
        env = {k:v for k,v in os.environ.items() if not k.startswith('DIVE_SIM_')}
        env.update(SDL_VIDEODRIVER='dummy', DIVE_WATCH_STATE_DIR=tmp,
                   DIVE_SIM_CAPTURE=str(prefix), DIVE_SIM_BENCH_CHECK='startup',
                   DIVE_SIM_BENCH_PROFILE=str(source))
        result = subprocess.run([str(binary),'--stop_at=24','--no-rt'],env=env,
                                text=True,capture_output=True,timeout=60)
        (evidence/f'{prefix.name}-native.txt').write_text(result.stdout+result.stderr)
        assert result.returncode == 0, result.stdout+result.stderr
        assert 'STARTUP SETTLED CAPTURE PASS' in result.stdout
        assert path.read_bytes() == preference
    assert source.read_bytes() == original
    state = json.loads(Path(f'{prefix}-10.json').read_text())
    states.append(state)
    rows = list(csv.DictReader(Path(f'{prefix}-10.csv').open()))
    accepted.append(Path(f'{prefix}-10.csv').read_bytes())
    assert [int(row['timestamp_seconds']) for row in rows] == list(range(11))
    # Supplied keyframes survive exactly at their timestamps, with float32 input representation.
    for index, pressure in [(0,0.9),(2,2.86133),(10,2.86133)]:
        assert abs(float(rows[index]['absolute_pressure_bar'])-pressure) < 1e-6
    assert state['surface_bar'] == float(rows[0]['absolute_pressure_bar'])
    assert state['density_kg_m3'] == density
    assert abs(state['depth_m']-depth) < 1e-4
    assert state['watch']['depth_dm'] == watch_dm
    assert state['replay'] and state['seconds'] == 10
    image = Image.open(f'{prefix}-10.bmp').convert('RGB')
    assert image.size == (1200,800)
    image.save(f'{prefix}-10.png')
    assert len(image.crop((22,32,342,332)).getcolors(100000) or []) > 10
assert accepted[0] == accepted[1], 'CSV import rewrote pressure/time for density'
for key in ('tissues','gf_ceilings','ceiling_meters','ndl_seconds','tts_seconds','stop_seconds','validity'):
    assert states[0][key] == states[1][key], key
assert abs(states[0]['depth_m']/states[1]['depth_m']-1.025) < 1e-6
(evidence/'input-sha256.txt').write_text(f'{digest}  pressure-input.csv\n')
print(f'CSV WATER IMPORT PASS: unchanged source SHA256 {digest}, exact accepted trace/keyframes/tissues, two restored waters, physical depths and settled frames')
