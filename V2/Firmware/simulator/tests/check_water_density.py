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
