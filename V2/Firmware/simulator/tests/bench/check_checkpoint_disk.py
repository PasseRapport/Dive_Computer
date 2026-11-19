#!/usr/bin/env python3
"""Public bench in separate OS processes; byte mutations are external input faults."""
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zlib
exe, legacy = map(Path, sys.argv[1:])
def run(*args):
    return subprocess.run([str(exe), *map(str, args)], check=True, text=True, capture_output=True).stdout
with tempfile.TemporaryDirectory(prefix="dive-checkpoint-") as d:
    root = Path(d)
    saved = root / "locked.checkpoint"
    run("save", saved)
    original = saved.read_bytes()
    assert len(original) == 364
    assert run("control", saved) == run("resume", saved), "continued observations diverged"
    run("preferred", saved)
    run("fault", saved, 4, "clock")
    run("fault", saved, 5, "gap")
    run("fault", saved, 3, "stale")
    run("fault", root / "missing", 1, "normal")
    mutations = [(70, 1, False, 1), (4, 4, True, 2), (64+8, 0, True, 2),
                 (64+289, 0, True, 6), (19, 0, True, 6), (16, 0, True, 6), (56, 1, True, 6), (59, 255, True, 6), (60, 1, True, 6)]
    for index, value, repair, expected in mutations:
        data = bytearray(original)
        data[index] = value
        if repair:
            if index >= 64:
                struct.pack_into("<I", data, 64+292, zlib.crc32(data[64:64+292]))
            struct.pack_into("<I", data, len(data)-4, zlib.crc32(data[:-4]))
        bad = root / f"fault-{index}.checkpoint"
        bad.write_bytes(data)
        run("fault", bad, expected, "normal")
    root.chmod(0o500)
    try:
        run("write-failure", saved)  # cannot create temp: original saved file must survive
    finally:
        root.chmod(0o700)
    assert saved.read_bytes() == original
    missing_identity=bytearray(original)
    missing_identity[48:56]=bytes(8)
    struct.pack_into("<I",missing_identity,len(missing_identity)-4,zlib.crc32(missing_identity[:-4]))
    path=root/'missing-identity.checkpoint'
    path.write_bytes(missing_identity)
    run('fault',path,6,'normal')
    previous = bytearray(original)
    struct.pack_into("<I", previous, 4, 1)
    previous[48:64] = bytes(16)
    struct.pack_into("<I", previous, len(previous)-4, zlib.crc32(previous[:-4]))
    old_pc = root / "schema-1.checkpoint"
    old_pc.write_bytes(previous)
    run("fault", old_pc, 8, "normal")
    assert old_pc.read_bytes() == previous
    run("write-failure", root / "absent" / "recovery.checkpoint")
    run("write-failure", root)  # rename cannot replace the existing directory
    assert saved.read_bytes() == original
    assert not list(root.glob("*.tmp.*"))
    old = legacy.read_bytes()
    assert hashlib.sha256(old).hexdigest() == "7c1286376c92dce9d2f524946fd72b26e844267e9eec1c0b2f10d6663f548c6a"
    run("legacy", legacy)
    assert legacy.read_bytes() == old, "legacy source was rewritten"
    unknown = bytearray(old)
    struct.pack_into("<I", unknown, 8, 0)
    struct.pack_into("<I", unknown, 289, zlib.crc32(unknown[:289]))
    path = root / "unknown-legacy"
    path.write_bytes(unknown)
    run("fault", path, 2, "normal")
print("DISK CHECKPOINT PASS: separate processes; exact tissues/anchor/NDL/TTS/stops/lifecycle continuity; explicit clock/exposure/configuration; locked recovery, later unlock; preferred independence; immutable recognized legacy; corruption/schema/policy/GF/lifecycle; atomic write/load failure")
