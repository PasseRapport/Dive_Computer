#!/usr/bin/env python3
"""Supplement native SDL click assertions with captured visible recovery states."""
import json
from pathlib import Path
import sys
from PIL import Image
base = Path(sys.argv[1])
frames = [json.loads(Path(f"{base}-{i}.json").read_text()) for i in range(4)]
assert frames[0]["gap"] and frames[0]["validity"] == [3]*5
assert "Unknown restart time" in frames[0]["recovery"]
assert frames[1]["gf"]["preferred"] == [20,90] and frames[1]["seconds"] == 0
assert frames[2]["gf"]["active"] == [40,85] and frames[2]["gf"]["preferred"] == [20,90]
assert frames[2]["gf"]["locked"] and frames[2]["recording"]["active"]
assert not frames[2]["gap"] and frames[2]["seconds"] == 1260
assert not frames[3]["gf"]["locked"] and frames[3]["recording"]["dives_completed"] == 1
assert frames[3]["gf"]["active"] == [20,90] and frames[3]["storage"]["log_count"] == 1
for i in range(4):
    path = Path(f"{base}-{i}.png")
    if not path.exists():
        path = Path(f"{base}-{i}.bmp")
    image = Image.open(path).convert("RGB")
    assert image.size == (1200,800)
    # Disk save/load/clock-selector buttons must have visible pixels within the
    # display, below the graph, and retain readable foreground text.
    for x1,x2 in [(636,762),(772,898),(908,1178)]:
        colors = list(image.crop((x1,764,x2,798)).get_flattened_data())
        assert sum(b>140 and b>r*1.1 for r,g,b in colors)>300
        assert sum(min(r,g,b)>210 for r,g,b in colors)>20
print("DISK RENDER PASS: four native clicked states, unknown clock INVALID, explicit saved instant, restored locked pair/independent preference, completion unlock and saved log; all three disk controls visible")
