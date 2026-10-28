# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual process restart; equality is continuity evidence, not a numerical oracle."""
from pathlib import Path
import math
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory() as directory:
    checkpoint = str(Path(directory) / "exposure.bin")
    control = subprocess.check_output([sys.argv[1], "--save", checkpoint], text=True)
    recovered = subprocess.check_output([sys.argv[1], "--resume", checkpoint], text=True)
    assert len(Path(checkpoint).read_bytes()) == 296
    assert len(control.splitlines()) == len(recovered.splitlines()) == 4
    for a, b in zip(control.splitlines(), recovered.splitlines()):
        left, right = a.split(), b.split()
        assert len(left) == len(right) == 95
        for index, (x, y) in enumerate(zip(left, right)):
            if 2 <= index <= 5 or 8 <= index < 40:
                assert math.isclose(float(x), float(y), rel_tol=0, abs_tol=1e-6), (index, x, y)
            else:
                assert int(x) == int(y), (index, x, y)
print("Fresh-process checkpoint/restart matches uninterrupted exposure")
