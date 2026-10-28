#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix="dive-recovery-lifecycle-") as directory:
    root=Path(directory)
    subprocess.run([sys.argv[1],str(root/'state'),str(root/'locked.checkpoint')],check=True)
    legacy=root/'legacy-state'
    legacy.mkdir()
    original=b'DIVE_WATCH_LOG 1 1 40 90 20 30 70 0 preserved-historical-policy\n'
    old=legacy/'dive-00000000000000000001.log'
    old.write_bytes(original)
    subprocess.run([sys.argv[1],str(legacy),str(root/'unused'),'legacy-read'],check=True)
    assert old.read_bytes()==original
    for mode in ('disk','memory'):
        state=root/mode
        checkpoint=root/f'{mode}.checkpoint'
        subprocess.run([sys.argv[1],str(state),str(checkpoint),mode],check=True)
        original={p.name:p.read_bytes() for p in state.glob('dive-*.log')}
        subprocess.run([sys.argv[1],str(state),str(checkpoint),'reopen'],check=True)
        assert {p.name:p.read_bytes() for p in state.glob('dive-*.log')}==original
print('RECOVERY INTEGRATION PASS: separate processes, identical interrupted/uninterrupted results across two dives, durable exact-pair completion logs, repeated recovery idempotence, independent identical fresh sessions')
