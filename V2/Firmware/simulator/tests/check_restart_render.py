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
def count(frame, box, predicate):
    return sum(predicate(*frame.getpixel((x, y)))
               for y in range(box[1], box[3]) for x in range(box[0], box[2]))

white = lambda r, g, b: min(r, g, b) > 190
red = lambda r, g, b: r > 230 and g < 110 and b < 110

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
        # The current watch contract is white INVALID beneath depth, plus a
        # red NO / DATA badge replacing numerical decompression guidance.
        marker = sum(1 for y in range(486,668) for x in range(1166,1170)
                  if (lambda c:c[0]>230 and c[1]<160 and c[2]<160)(frame.getpixel((x,y))))
        assert marker > 150, (index, "missing red history-gap marker", marker)
        # The shortest caption is just INVALID; exposure gaps append details.
        assert count(frame, (132,139,234,150), white) > 40, (index, "missing INVALID caption")
        assert count(frame, (74,178,168,238), red) > 2500, (index, "missing red invalid-data badge")
        assert count(frame, (90,185,151,204), white) > 100, (index, "missing NO line")
        assert count(frame, (89,210,151,229), white) > 150, (index, "missing DATA line")
    else:
        assert not state["gap"] and state["seconds"] == 0
        assert state["validity"] == [1,0,0,0,0]
    if index in (0, 6):
        assert count(frame, (74,178,168,238), red) < 50, (index, "invalid-data badge on valid/fresh exposure")
    # Each reason is rendered in the diagnostics row, not merely JSON metadata.
    text = sum(1 for y in range(132,150) for x in range(636,1180)
               if min(frame.getpixel((x,y)))>175)
    assert text > 150, (index, text)
print("RESTART RENDER PASS: seven reasons, valid recovery, white INVALID/red NO DATA faults and fresh fixture")
