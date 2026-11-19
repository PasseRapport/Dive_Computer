"""Durable behavior through the public configure/profile/observe bench seam."""
import subprocess
import sys
import tempfile
from pathlib import Path
binary = sys.argv[1]
def run(state, *args):
    return subprocess.check_output([binary, str(state), *args], text=True).strip()
with tempfile.TemporaryDirectory(prefix="dive-storage-test-") as tmp:
    state = Path(tmp) / "state"
    assert run(state, "read").split()[:4] == ["30", "70", "0", "0"]
    assert run(state, "set", "40", "85").split()[:4] == ["40", "85", "0", "1"]
    assert run(state, "read").split()[:4] == ["40", "85", "1", "1"]
    run(state, "dives")
    records = run(state, "logs").splitlines()
    # Marine physical 2 m is +.201036325 bar: .005 bar/s crosses at
    # seconds 41 and 111. Surface .5 m is +.05025908125 bar: the
    # second ascent enters surface at 145, completing 300 samples at 444.
    assert records[0].startswith("LOG 1 20 60 41 90 0 "), records
    assert records[1].startswith("LOG 2 70 70 111 444 1 "), records
    assert "configurable" in records[0], records
    original = {p.name: p.read_bytes() for p in state.glob("dive-*.log")}
    run(state, "set", "100", "100")
    assert {p.name: p.read_bytes() for p in state.glob("dive-*.log")} == original
    assert run(state, "logs").splitlines()[:2] == records[:2]
    run(state, "load-preserves")
    run(state, "fail", str(Path(tmp) / "last-good"))
    assert run(state, "read").split()[:4] == ["100", "100", "1", "1"]
    assert {p.name: p.read_bytes() for p in state.glob("dive-*.log")} == original
    legacy = state / "historical-v0.log"
    legacy.write_bytes(b"Historical dive without GF provenance\n")
    corrupt = state / "dive-00000000000000000003.log"
    corrupt.write_bytes(b"unrecognized legacy or corrupt data\n")
    run(state, "set", "1", "1")
    assert legacy.read_bytes() == b"Historical dive without GF provenance\n"
    assert corrupt.read_bytes() == b"unrecognized legacy or corrupt data\n"
    run(state, "legacy")
    for contents in [b"", b"garbage", b"DIVE_WATCH_GF 2 30 70\n",
                     b"DIVE_WATCH_GF 1 0 70\n", b"DIVE_WATCH_GF 1 71 70\n",
                     b"DIVE_WATCH_GF 1 30 101\n", b"DIVE_WATCH_GF 1 30 70 extra\n",
                     b"DIVE_WATCH_GF 1 4294967297 70\n"]:
        (state / "preference.txt").write_bytes(contents)
        run(state, "invalid")
print("SEPARATE PROCESS STORAGE PASS")
