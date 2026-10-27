#!/usr/bin/env python3
"""Verify native captures: profile/time marker and separately drawn exposure.

Usage: python3 check_bench_render.py /tmp/bench-visible
Requires Pillow; reads PREFIX-{0..3}.bmp and corresponding .json observations.
"""
import json
import sys
from pathlib import Path
from PIL import Image

prefix = sys.argv[1]
for index, seconds in enumerate((1920, 3600, 4920, 5040)):
    state = json.loads(Path(f"{prefix}-{index}.json").read_text())
    assert state["seconds"] == seconds and state["duration"] == 5040, state
    assert len(state["tissues"]) == 16
    frame = Image.open(f"{prefix}-{index}.bmp").convert("RGB")
    assert frame.size == (1200, 800)
    # Graph bounds from the visible layout contract: time from x=62 to 1168.
    expected_x = round(62 + seconds / 5040 * 1106)
    marker = sum(
        1 for x in range(expected_x - 2, expected_x + 3)
        for y in range(486, 668)
        if (lambda rgb: rgb[0] > 230 and 170 < rgb[1] < 230 and rgb[2] < 150)(frame.getpixel((x, y)))
    )
    assert marker > 150, (index, expected_x, marker)
    # Independent hydrostatic worked depth: +3 bar at 1025 kg/m3 is
    # 29.8448648 m, so the first plateau lies at y=577.53.
    actual = frame.getpixel((300, 577))
    assert actual[1] > 180 and actual[2] > 170 and actual[0] < 150, actual
    assert all(any((lambda c: c[1]>180 and c[2]>170 and c[0]<150)(frame.getpixel((x,y)))
                   for y in range(576,580)) for x in range(100,450)), "Plateau has holes"
    if index == 1:
        assert 0.9 < state["tissues"][5] < 1.8, "surface interval lost loading"
gap = Image.open(f"{prefix}-4.bmp").convert("RGB")
assert sum(1 for x in range(1164,1172) for y in range(486,668)
           if (lambda c: c[0]>230 and c[1]<150 and c[2]<150)(gap.getpixel((x,y)))) > 150
manual = Image.open(f"{prefix}-5.bmp").convert("RGB")
assert sum(1 for x in range(612,618) for y in range(540,575)
           if (lambda c: c[1]>180 and c[2]>170 and c[0]<150)(manual.getpixel((x,y)))) > 20, "Brief manual depth excursion disappeared"
print("BENCH RENDER PASS: 4 synchronized markers, downward depth, actual trace, retained loading, gaps and brief manual excursion")
