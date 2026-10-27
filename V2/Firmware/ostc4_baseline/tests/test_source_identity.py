# SPDX-License-Identifier: GPL-3.0-or-later
"""Byte identity against the manifest pinned from the completed reuse audit."""
import hashlib
import json
import pathlib
import sys
root = pathlib.Path(sys.argv[1])
manifest = json.loads((root / 'upstream-manifest.json').read_text())
assert manifest['revision'] == '7198d203520f3a3cdccf1ccc0bf8af6475ee4819'
for name, digest in manifest['files'].items():
    assert hashlib.sha256((root / 'upstream' / name).read_bytes()).hexdigest() == digest, name
assert len(manifest['files']) == 11
print('all 11 selected upstream files retain their exact audited bytes')
