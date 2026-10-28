# SPDX-License-Identifier: GPL-3.0-or-later
"""Package byte-preserved fetched/compiled audit inputs once; refuse replacement."""
from pathlib import Path
import hashlib
import json
import tarfile

root = Path('/tmp/ticket09/evidence')
archive_path = root / 'raw-evidence.tar.gz'
index_path = root / 'raw-evidence-index.json'
if archive_path.exists() or index_path.exists():
    raise SystemExit('Evidence already packaged; use a fresh collection directory. Existing evidence preserved.')
raw = sorted(p for p in root.rglob('*') if p.is_file() and (
    p.suffix in {'.txt', '.map', '.log', '.config'} or p.name == 'compile_commands.json'))
if not raw:
    raise SystemExit('No raw evidence found; nothing written.')
index = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in raw}
with tarfile.open(archive_path, 'w:gz') as archive:
    for path in raw:
        archive.add(path, arcname=path.relative_to(root))
with tarfile.open(archive_path) as archive:
    for name, expected in index.items():
        if hashlib.sha256(archive.extractfile(name).read()).hexdigest() != expected:
            raise SystemExit('Archive verification failed; raw files preserved.')
index_path.write_text(json.dumps(index, indent=2) + '\n')
for path in raw:
    path.unlink()
