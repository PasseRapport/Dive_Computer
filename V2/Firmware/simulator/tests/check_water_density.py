#!/usr/bin/env python3
"""Public water journey captures. Expected values are independent spec literals.
Usage: python3 check_water_density.py /tmp/water-density
The native journey checks inputs, UI commits/cancellation, locks and exposure;
this checker verifies exported observations paired with rendered captures.
"""
import json
import sys
from pathlib import Path
from PIL import Image

prefix = sys.argv[1]
for index, (surface, density, depth, watch_dm) in enumerate((
    (1.01325, 1000, 20.0, 200), (1.01325, 1025, 19.51219512, 195),
    (0.9, 1000, 20.0, 200), (0.9, 1025, 19.51219512, 195),
)):
    state = json.loads(Path(f"{prefix}-{index}.json").read_text())
    assert abs(state["surface_bar"] - surface) < 1e-6, state
    assert state["density_kg_m3"] == density, state
    assert abs(state["absolute_pressure_bar"] - surface - 1.96133) < 1e-6, state
    assert abs(state["depth_m"] - depth) < 1e-4, state
    assert state["watch"]["depth_dm"] == watch_dm, state
    frame = Image.open(f"{prefix}-{index}.bmp").convert("RGB")
    assert frame.size == (1200, 800)
    # Both watch and bench are present and contain rendered text/graphics.
    white = lambda rgb: min(rgb) > 220
    assert sum(white(pixel) for pixel in frame.crop((40, 50, 320, 315)).getdata()) > 500, "Watch frame is blank"
    assert sum(white(pixel) for pixel in frame.crop((630, 35, 1175, 145)).getdata()) > 300, "Bench summary is blank"
print("WATER DENSITY PASS: two explicit surfaces, both densities, physical bench/watch depths and paired frames")

# Physical trajectory and public recording/alert observations agree in both
# waters. Tolerances remain 1e-4 m for binary32 conversion; displayed dm exact.
for salt, density in enumerate((1000, 1025)):
    for offset, (depth, dm) in enumerate(((2, 20), (10, 100), (11, 110), (10.9, 109), (.499, 4))):
        index = 4 + salt * 5 + offset
        state = json.loads(Path(f"{prefix}-{index}.json").read_text())
        assert state["density_kg_m3"] == density, state
        assert abs(state["depth_m"] - depth) < 1e-4, state
        assert state["watch"]["depth_dm"] == dm, state
        assert state["recording"]["density_kg_m3"] == density, state
        assert state["recording"]["max_depth_dm"] == (20 if offset==0 else 100 if offset==1 else 110), state
        assert state["recording"]["active"] and state["recording"]["started_seconds"] == 2, state
        assert state["recording"]["surface_seconds"] == (1 if offset == 4 else 0), state
        frame = Image.open(f"{prefix}-{index}.bmp").convert("RGB")
        # A physical metre has the same downward graph scale in each water.
        # Existing graph y bounds are 488..668 for 0..60 m. Inspect the
        # penultimate sample, away from the yellow current-time marker.
        previous_seconds, previous_depth = ((1, 1.999), (3, 9.999), (4, 10), (14, 11), (16, .5))[offset]
        x = round(62 + previous_seconds / state["seconds"] * 1106)
        y = round(488 + previous_depth * 3)
        assert any((lambda c: c[0] < 150 and c[1] > 180 and c[2] > 170)(frame.getpixel((xx, yy)))
                   for xx in range(x - 2, x + 3) for yy in range(y - 2, y + 3)), (index, x, y)

for salt, density in enumerate((1000, 1025)):
    for stage, depth, current_max, session_max, last, dives, active in (
        (3, 123, 123, 123, 0, 1, True),
        (6, 2, 123, 123, 123, 1, False),
        (10, 0, 50, 123, 50, 2, False),
    ):
        index = salt * 12 + stage
        state = json.loads(Path(f"{prefix}-apnea-{index}.json").read_text())
        assert state == dict(density_kg_m3=density, last_density_kg_m3=0 if stage==3 else density, depth_dm=depth,
                             current_max_depth_dm=current_max, max_depth_dm=session_max,
                             last_depth_dm=last, total_dives=dives,
                             total_underwater_s={3: 1, 6: 4, 10: 6}[stage],
                             elapsed_s={3: 1, 6: 4, 10: 2}[stage],
                             max_dive_s={3: 1, 6: 4, 10: 4}[stage], active=active), state
        frame = Image.open(f"{prefix}-apnea-{index}.bmp").convert("RGB")
        # Existing decimal anchor is fixed at watch x=160,y~95. This verifies
        # the physical-depth glyph renders in the actual freediving watch.
        assert sum(min(frame.getpixel((22+x,32+y))) > 220
                   for x in range(157,164) for y in range(89,101)) > 8, index
print("WATER RECORDING/APNEA PASS: physical threshold/rate captures, graph geometry, two-density session depth records and decimal anchors")
