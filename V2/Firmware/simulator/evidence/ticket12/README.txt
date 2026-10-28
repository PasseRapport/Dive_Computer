Ticket 12: atomic configurable GF preparation and recorded-dive lock
================================================================

PC synthetic evidence only. No target or physical-board validation.

headless-tests.log: full 18/18 PC CTest run, including original 13 core checks.
checkpoint-validation.log: added malformed GF pair/lock checkpoint checks.
independent-comparison.json: 12 frozen cases/114 events plus five independent 1 s
shallow cases/455 observations. No TTS differences; no numerical failures. Raw public observations are retained
in independent-observations.jsonl.
Frozen independent source/policy/provenance: ../../tests/bench/gf-reference.
Only total TTS permits the established 3 s allowance; NDL/stop slots/validity exact.
Original fixed 30/70 evidence and former exact-TTS rejection are unchanged.

native-gf-demo.log/render-check.log: native Zephyr/SDL demonstration and pixel/state
checks. gf-0..5 PNG/JSON/CSV show initial preparation, separate Low draft, complete
pair review, committed 40/85 preparation, start during unfinished draft with 40/85
locked, then completed/unlocked preparation. CSV files are exact accepted input
segments; this demo deliberately uses the public nonunit segment cadence and
therefore has three history points at 90 s, not an invented one-second trace.

Primary test seam: public PC bench configuration, timestamped pressure input and
observations. Matched-control/prediction tests preserve checkpoint bytes and later
anchor-sensitive outputs. Checkpoint v2 is 296 bytes, with saved pair and lock;
known controller state is restored consistently from memory. Subsequent GF/manual
controller changes invalidate pressure-only recovery eligibility while preserving
the old checkpoint bytes, with an explicit reason. Disk persistence and legacy
migration are later tickets; endpoint calculations/display are ticket 13.
