# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the explicit correction set only to an exactly pinned build copy."""
import hashlib
import json
import pathlib
import sys

root, output = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
source = root / 'upstream/Common/Src/decom.c'
original = source.read_bytes()
manifest = json.loads((root / 'upstream-manifest.json').read_text())
if hashlib.sha256(original).hexdigest() != manifest['files']['Common/Src/decom.c']:
    raise SystemExit('refusing to patch changed upstream source')
corrections = json.loads((root / 'corrections/tissue-corrections.json').read_text())
patched = original
for correction in corrections['changes']:
    old, new = correction['original'].encode(), correction['corrected'].encode()
    if patched.count(old) != 1:
        raise SystemExit('correction literal must occur exactly once: ' + correction['original'])
    patched = patched.replace(old, new, 1)
block = corrections['schreiner']
old = (root / 'corrections' / block['original_file']).read_bytes()
new = (root / 'corrections' / block['corrected_file']).read_bytes()
if (hashlib.sha256(old).hexdigest() != block['original_sha256'] or
        hashlib.sha256(new).hexdigest() != block['corrected_sha256']):
    raise SystemExit('changed Schreiner correction definition')
# Match the original line endings; preserve every untouched source byte.
if b'\r\n' in original:
    old, new = old.replace(b'\n', b'\r\n'), new.replace(b'\n', b'\r\n')
if patched.count(old) != 1:
    raise SystemExit('Schreiner correction must match exactly once')
patched = patched.replace(old, new, 1)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(patched)
