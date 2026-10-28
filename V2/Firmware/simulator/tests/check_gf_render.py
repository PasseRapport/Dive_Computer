#!/usr/bin/env python3
"""Native GF editor/lock captures; presentation evidence, no numerical oracle."""
import csv
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops
prefix = sys.argv[1]
states, frames = [], []
for index in range(6):
    state = json.loads(Path(f'{prefix}-{index}.json').read_text())
    path = Path(f'{prefix}-{index}.png')
    frame = Image.open(path if path.exists() else f'{prefix}-{index}.bmp').convert('RGB')
    assert frame.size == (1200,800)
    with Path(f'{prefix}-{index}.csv').open() as stream:
        samples = list(csv.DictReader(stream))
    assert len(samples) == state['history_count']
    assert int(samples[-1]['timestamp_seconds']) == state['seconds']
    gf = state['gf']
    assert gf['policy'] in ('ostc4-continuous-air-configurable-gf-retained-anchor-v2',
                            'ostc4-continuous-air-configurable-gf-retained-anchor-grid-time-v3')
    assert 'gf30-70-v1' in gf['legacy_comparison_policy']
    assert gf['locked'] == state['recording']['active']
    states.append(state)
    frames.append(frame.crop((22,32,342,332)))
assert states[0]['gf']['preferred'] == [30,70]
assert states[1]['gf']['draft'] == [40,70] and states[1]['gf']['preferred'] == [30,70]
assert states[2]['gf']['draft'] == [40,85] and states[2]['gf']['preferred'] == [30,70]
assert states[3]['gf']['preferred'] == [40,85] and not states[3]['gf']['editing']
assert states[4]['gf']['active'] == [40,85] and states[4]['gf']['locked'] and not states[4]['gf']['editing']
assert states[5]['gf']['preferred'] == [40,85] and not states[5]['gf']['locked']
assert states[5]['seconds'] == 90 and states[5]['history_count'] == 3
# Preparation pair and both editor phases must render visible different content.
for first,second in [(0,1),(1,2),(0,3),(3,4),(4,5)]:
    difference = ImageChops.difference(frames[first],frames[second])
    assert sum(1 for pixel in difference.getdata() if max(pixel)>50)>200, (first,second)
print('GF RENDER PASS: six preparation/draft/review/committed/locked/unlocked states and exact exposure traces')
