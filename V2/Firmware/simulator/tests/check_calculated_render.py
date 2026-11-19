#!/usr/bin/env python3
"""Check the native synthetic watch/diagnostics/graph captures and exact inputs.

Usage: python3 check_calculated_render.py /tmp/calculated-prefix
Accepts PNG (published) or BMP (fresh native capture). Not a numerical oracle.
"""
import csv
import json
import math
from pathlib import Path
import sys
from PIL import Image
prefix = sys.argv[1]
frames = []
for index in range(10):
    state = json.loads(Path(f"{prefix}-{index}.json").read_text())
    path = Path(f"{prefix}-{index}.png")
    frame = Image.open(path if path.exists() else f"{prefix}-{index}.bmp").convert("RGB")
    frames.append(frame)
    assert frame.size == (1200, 800)
    with Path(f"{prefix}-{index}.csv").open() as stream:
        samples = list(csv.DictReader(stream))
    assert len(samples) == state["history_count"] == state["seconds"] + 1
    assert [int(row["timestamp_seconds"]) for row in samples] == list(range(state["seconds"]+1))
    assert math.isclose(float(samples[-1]["absolute_pressure_bar"]), state["absolute_pressure_bar"], abs_tol=1e-8)
    watch = state["watch"]
    if "recording" in state:  # Published pre-lifecycle captures remain readable.
        for field in ("active", "armed", "dive_seconds", "surface_seconds"):
            assert watch[field] == state["recording"][field], (index, field)
    if watch["state"] in (1, 2):
        assert watch["ndl_seconds"] == state["ndl_seconds"]
        assert watch["tts_seconds"] == state["tts_seconds"]
    # Position is driven by accepted exposure time, independent of gauge rounding.
    x = round(62 + state["seconds"] / state["duration"] * 1106)
    marker = sum(1 for px in range(x-2, x+3) for y in range(486,668)
        if (lambda c: c[0]>230 and 170<c[1]<230 and c[2]<150)(frame.getpixel((px,y))))
    if index != 6:
        assert marker>150, (index, marker)
    if index == 0: assert watch["state"] == 0
    if index == 3: assert state["automatic"] and watch["stop_depth_dm"] == watch["depth_dm"] == 119
    if index == 4: assert not state["automatic"] and watch["alerts"] & 1 and watch["notifications"] & 1
    if index == 5: assert watch["alerts"] & 1 and not watch["notifications"] & 1 and watch["stop_depth_dm"]>0
    if index == 6: assert watch["state"] == 3 and all(v==3 for v in state["validity"])
    if index == 7: assert state["tissues"][5]>.9
    if index == 9:
        assert state["physical"]["ceiling_m"] < watch["depth_dm"]/10 < watch["stop_depth_dm"]/10
        assert not watch["alerts"] & 1
# The ceiling warning track must not use the deeper first mandatory stop.
def red_track(frame):
    return sum(1 for x in range(30,334) for y in range(32,332)
        if 142**2 < (x-182)**2+(y-182)**2 < 150**2
        and (lambda c:c[0]>180 and c[1]<110 and c[2]<110)(frame.getpixel((x,y))))
assert red_track(frames[4])>500
assert red_track(frames[9])==0
print("CALCULATED RENDER PASS: 10 states, exact pressure traces, gauge/diagnostic mapping, graph markers, acknowledgement and true ceiling track")
