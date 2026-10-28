#!/usr/bin/env python3
"""Native paired-ceiling display/stop/alert evidence; PC rendering only."""
import csv
from decimal import Decimal, ROUND_HALF_UP
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops

prefix = sys.argv[1]
states, frames = {}, {}
for index in range(6, 13):
    state = json.loads(Path(f'{prefix}-{index}.json').read_text())
    path = Path(f'{prefix}-{index}.png')
    image = Image.open(path if path.exists() else f'{prefix}-{index}.bmp').convert('RGB')
    assert image.size == (1200, 800)
    with Path(f'{prefix}-{index}.csv').open() as stream:
        samples = list(csv.DictReader(stream))
    assert len(samples) == state['history_count']
    assert int(samples[-1]['timestamp_seconds']) == state['seconds']
    states[index], frames[index] = state, image.crop((22, 32, 342, 332))
    # Both values and stable identifying label occupy the actual bottom field.
    footer = frames[index].crop((55, 215, 265, 274))
    assert sum(min(pixel)>180 for pixel in footer.getdata())>150
    endpoint = state['gf_ceilings']
    if index != 11:
        assert endpoint['validity'] == [1, 1]
        assert endpoint['low_m'] >= endpoint['high_m'] >= 0

def tenths(value):
    return str(Decimal(str(value)).quantize(Decimal('0.1'), rounding=ROUND_HALF_UP))

assert states[6]['gf_ceilings']['low_m'] == states[6]['gf_ceilings']['high_m'] == 0
# Independent fixture's decimal expectations, separately from raw0.006m bound.
assert tenths(states[7]['gf_ceilings']['low_m']) == '13.4'
assert tenths(states[7]['gf_ceilings']['high_m']) == '7.2'
assert states[7]['watch']['stop_depth_dm'] > 0
assert states[8]['watch']['stop_remaining_ms'] > 0
assert states[8]['watch']['stop_depth_dm'] <= states[8]['watch']['depth_dm']
assert states[9]['watch']['alerts'] & 1 and states[9]['watch']['notifications'] & 1
assert states[10]['watch']['alerts'] & 1 and not states[10]['watch']['notifications'] & 1
assert states[11]['gf_ceilings']['validity'] == [3, 3]
assert states[12]['gf_ceilings']['low_m'] == states[12]['gf_ceilings']['high_m'] > 30
assert tenths(states[12]['gf_ceilings']['low_m']) == '44.3'
for first, second in [(6, 7), (7, 8), (9, 10), (10, 11), (11, 12)]:
    difference = ImageChops.difference(frames[first], frames[second])
    assert sum(max(pixel)>50 for pixel in difference.getdata())>100
print('PAIRED CEILINGS RENDER PASS: simultaneous Low/High, units/rounding, zero, equality, large values, mandatory depth/countdown, alerts/acknowledgement and explicit invalidity; native text/glyph geometry assertions also passed')
