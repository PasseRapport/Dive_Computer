# SPDX-License-Identifier: GPL-3.0-or-later
"""Actual process restart; equality is continuity evidence, not a numerical oracle."""
from pathlib import Path
import math
import subprocess
import sys
import tempfile
import struct
import zlib
with tempfile.TemporaryDirectory() as directory:
    checkpoint = str(Path(directory) / "exposure.bin")
    control = subprocess.check_output([sys.argv[1], "--save", checkpoint], text=True)
    recovered = subprocess.check_output([sys.argv[1], "--resume", checkpoint], text=True)
    assert len(Path(checkpoint).read_bytes()) == 296
    # A structurally valid checkpoint carrying the previous numerical identity
    # cannot silently continue under the corrected stop-travel arithmetic.
    prior_identity = (b"7198d203520f3a3cdccf1ccc0bf8af6475ee4819\n"
        b"ostc4-beta-7198d20-tissue-corrected-air-gf30-70-v1\n"
        b"cached-10s-and-stable-schreiner-v1\n"
        b"ostc4-continuous-air-configurable-gf-retained-anchor-v2\n"
        b"ieee-binary32-binary64-no-fast-math-no-contract-checkpoint-v2")
    old_policy = zlib.crc32(prior_identity)
    prior = bytearray(Path(checkpoint).read_bytes())
    assert struct.unpack_from('<I',prior,8)[0] != old_policy
    struct.pack_into('<I',prior,8,old_policy)
    struct.pack_into('<I',prior,len(prior)-4,zlib.crc32(prior[:-4]))
    prior_path = Path(directory)/'prior-policy.bin'
    prior_path.write_bytes(prior)
    rejected = subprocess.run([sys.argv[1], '--resume', str(prior_path)],capture_output=True,text=True)
    assert rejected.returncode != 0, 'prior numerical policy was silently accepted'
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
