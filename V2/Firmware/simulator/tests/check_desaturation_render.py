#!/usr/bin/env python3
"""Combine native exact-label/circle assertions with saved summary image evidence."""
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops
prefix = sys.argv[1]
frames, states = {}, {}
for index, (minutes, valid) in enumerate(((1343, True), (1343, True), (1342, True),
                                        (1340, True), (0, False), (0, True),
                                        (0, True), (3517, True), (1, True), (59, True),
                                        (60, True), (61, True), (754, True))):
    state = json.loads(Path(f'{prefix}-{index}.json').read_text())
    path = Path(f'{prefix}-{index}.png')
    frame = Image.open(path if path.exists() else f'{prefix}-{index}.bmp').convert('RGB')
    assert frame.size == (1200, 800)
    assert state['summary']['valid']
    assert state['summary']['desaturation_valid'] == valid
    assert state['summary']['desaturation_minutes'] == minutes
    assert state['desaturation'] == {'minutes': minutes if index < 8 else 3517, 'availability': 1 if valid else 3}
    watch = frame.crop((22, 32, 342, 332))
    # Native visible-label checks verify all six exact strings, the acute glyph,
    # nonoverlap and each label corner inside the circular aperture. These ink
    # checks additionally establish pixels actually reached all six saved rows.
    for top, bottom in ((40, 61), (82, 112), (124, 151), (165, 186), (203, 219), (235, 251)):
        band = watch.crop((30, top, 290, bottom))
        assert sum(min(c) > 180 for c in band.getdata()) > 40, (index, top)
    states[index], frames[index] = state, watch
assert states[0]['seconds'] == states[1]['seconds'] == 2400
assert frames[0].tobytes() == frames[1].tobytes(), 'pause changed summary'
assert states[2]['seconds'] == 2460 and states[3]['seconds'] == 2520
assert states[3]['seconds'] == states[4]['seconds'], 'invalidity waited for minute refresh'
for key in ('max_dm', 'average_dm', 'duration_seconds'):
    assert len({states[i]['summary'][key] for i in range(5)}) == 1
assert states[2]['watch']['surface_seconds'] == states[0]['watch']['surface_seconds'] + 60
assert states[3]['watch']['surface_seconds'] == states[2]['watch']['surface_seconds'] + 60
for a, b in ((0, 2), (2, 3), (3, 4), (6, 7)):
    assert ImageChops.difference(frames[a].crop((35, 235, 285, 251)),
                                frames[b].crop((35, 235, 285, 251))).getbbox()
assert frames[5].crop((35, 235, 285, 251)).tobytes() == frames[6].crop((35, 235, 285, 251)).tobytes()
assert states[6]['seconds'] - states[5]['seconds'] == 60
print('DESATURATION RENDER PASS: exact native labels/glyph/circular bounds; saved pixels for positive, live minute changes, reopened summary, immediate invalidity, persistent zero, 58-hour estimate and retained statistics/surface time')
