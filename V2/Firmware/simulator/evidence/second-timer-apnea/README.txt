Warmup: unavailable guidance remains --, with no UNAVAILABLE word in the watch zone. INVALID and NO DATA are retained for faults.
Rate -/+ changes vertical velocity by 1 m/min. Negative ascends; positive descends. Depth -/+ changes commanded depth by one metre and stops vertical movement. Both modes use the accepted bench depth; apnea duration uses the bench exposure clock, including pause and acceleration. Fresh-fixture clock rewinds reset apnea timing.
Mandatory-stop countdown: the engine still returns its unmodified ten-second schedule slots. Between changes, presentation counts down in exposure seconds only when both observations are in the same stop zone and guidance is valid. A changed slot rebases the timer, leaving/re-entering the zone rebases it, repeated timestamps do not advance it. Countdown retains at least one second while the engine still requires a stop. TTS, raw diagnostics, stop obligations and automatic ascent remain engine-controlled.
Validation: 16/16 Release host tests, including public presentation checks for consecutive ticks, unchanged raw schedule/TTS, pause, rebasing, out-of-zone return and non-clearing countdown. Native calculated, battery-wheel and apnea SDL controls passed; native restart recipe and calculated/restart/gap render checks passed. Warmup image zone has no text. Apnea captures show 6 m after Rate +1 for 60 simulation seconds, then 7 m after Depth +1; elapsed time advances from 1:00 to 1:01.
PC/native evidence only; no target or physical execution.

Standards review: zero findings.
Spec review: zero findings.
Independent parallel reviews compared base 0aa254b349d51ca44abefe8df1280225bdd53b9c to implementation 140f3cf.

Source SHA256:
adb343b0cc31a2f2318a2c5e92f0ab210ccbff811fca63ee56306dc9abd7ffc4 launch.py
a52bd4f6bbb23e64d7017a1a2050ef01526e9c2d30fd76098def5cbbb93cc435 src/main.c
b12ffa297f874231eb999f717c9851eeb25a773ed787181b62df1a931d4c20b6 src/pc_bench_checks.c
112dc04e12b5604b59553ccf48c35436dbb82275e56b56ebdfeb0637347aabf4 src/scuba_calculated.c
a744dc4807fa63196364187c24aefafaf7b6423836f2a8c2abc6c945bc9797ea src/scuba_dashboard.c
3b861a6db0bd984b0fe5effafd51268dd23300b1070d0ed5d5dab15e5a46b649 src/scuba_model.h
ac73db0f2700a4e7cd4d1adc572638b1fc12863c74afcaaf7c8685f397bda633 src/sim_controls.c
b209baab2914ad264674f24c216794759479c7ffd88654d193b180e809714216 tests/bench/test_calculated_watch.c
