#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Restart capture consistency/presentation; no numerical correctness claim."""
from pathlib import Path
import json
import sys
from PIL import Image
prefix = sys.argv[1]
reasons = ["Recovered: complete exposure", "Unknown restart exposure", "Unknown restart time",
           "Corrupt checkpoint", "Incompatible checkpoint", "Stale checkpoint", "Fresh synthetic fixture"]
for index, reason in enumerate(reasons):
    state = json.loads(Path(f"{prefix}-{index}.json").read_text())
    path = Path(f"{prefix}-{index}.png")
    frame = Image.open(path if path.exists() else f"{prefix}-{index}.bmp").convert("RGB")
    assert frame.size == (1200, 800)
    assert state["recovery"] == reason
    if index == 0:
        assert state["validity"] == [1]*5 and state["watch"]["state"] == 1
        assert state["watch"]["tts_seconds"] == state["tts_seconds"] > 0
        assert state["watch"]["stop_depth_dm"] > 0
        assert state["history_count"] == state["seconds"]+1 == 2221
    elif index < 6:
        assert state["gap"] and state["validity"] == [3]*5 and state["watch"]["state"] == 3
        # Depth history position remains red, and INVALID is visibly yellow on watch.
        red = sum(1 for y in range(486,668) for x in range(1166,1170)
                  if (lambda c:c[0]>230 and c[1]<160 and c[2]<160)(frame.getpixel((x,y))))
        yellow = sum(1 for y in range(138,150) for x in range(150,215)
                     if (lambda c:c[0]>180 and c[1]>140 and c[2]<100)(frame.getpixel((x,y))))
        assert red > 150 and yellow > 20, (index, red, yellow)
    else:
        assert not state["gap"] and state["seconds"] == 0
        assert state["validity"] == [1,0,0,0,0]
    # Each reason is rendered in the diagnostics row, not merely JSON metadata.
    text = sum(1 for y in range(132,150) for x in range(636,1180)
               if min(frame.getpixel((x,y)))>175)
    assert text > 150, (index, text)
print("RESTART RENDER PASS: seven reasons, valid recovery, explicit INVALID faults and fresh fixture")
