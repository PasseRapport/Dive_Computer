#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Presentation evidence for missing-data time and graph abscissa, PC only."""
import json
from pathlib import Path
import sys
from PIL import Image
prefix = sys.argv[1]
frames = {}
for index in (0, 1, 2, 7):
    state = json.loads(Path(f"{prefix}-{index}.json").read_text())
    path = Path(f"{prefix}-{index}.png")
    frame = Image.open(path if path.exists() else f"{prefix}-{index}.bmp").convert("RGB")
    frames[index] = frame
    assert frame.size == (1200,800)
    gap = state["data_gap"]
    if index == 0:
        assert not gap["present"] and state["validity"] == [1]*5
    elif index == 1:
        assert gap == {"present":True,"time_known":True,"ongoing":False,"seconds":300}
    elif index == 2:
        assert gap["present"] and not gap["time_known"]
    else:
        assert gap == {"present":True,"time_known":True,"ongoing":True,"seconds":125}
        assert state["elapsed_seconds"] == 2345 and state["seconds"] == 2220
        assert state["history_count"] == 2221 and state["validity"] == [3]*5
        # Time continues to the red marker, but the known cyan pressure curve stops.
        cyan = sum(1 for x in range(1115,1160) for y in range(488,665)
                   if (lambda c:c[0]<150 and c[1]>180 and c[2]>170)(frame.getpixel((x,y))))
        assert cyan == 0, "Invented pressure during sensor loss"
    # All five time labels are readable within the plot's bottom margin.
    for left, right in ((62,150),(293,383),(570,660),(846,936),(1078,1168)):
        ink = sum(1 for x in range(left,right) for y in range(672,684)
                  if min(frame.getpixel((x,y)))>175)
        assert ink>15, (index,left,ink)
assert frames[1].crop((80,138,280,150)).tobytes() != frames[2].crop((80,138,280,150)).tobytes()
assert frames[7].crop((80,138,280,150)).tobytes() != frames[2].crop((80,138,280,150)).tobytes()
print("GAP TIME RENDER PASS: known 5:00 gap, unknown duration, ongoing 2:05 loss, five time ticks, no invented pressure")
