Ticket07 synthetic PC evidence, 2026-10-28

restart-0: loaded recovery using all retained exact pressure inputs.
restart-1..5: missing pressure history, unknown clock, corrupt bytes,
incompatible schema, stale checkpoint. All have explicit diagnostics and INVALID
watch guidance. restart-6: explicit fresh synthetic fixture, not recovered history.
regression-0..9: ticket06 calculated watch/automatic diver replay with final UI.
Every image has current observation/watch JSON and accepted input CSV. Faults are
explicit recipe events named in JSON, not complete exposure inferred from CSV.

16/16 Release headless checks pass; C checks use always-active CHECK assertions.
Fresh-process checkpoint replay compares to uninterrupted replay. All six existing
independently authored schedule profiles pass under the previously approved ±3s
TTS comparison policy; no new tolerance amendment or arithmetic change. Restart
comparisons themselves require exact NDL/TTS/stops/validity/anchors/pressure and
absolute 1e-6 tissues/ceiling. They prove continuity equivalence, not mathematics.
Native restart and calculated-watch recipes plus both rendering checkers pass.
Python syntax checks pass. See adjacent logs, source hashes and build identities.

Reproduce from repository root (use an output directory without spaces for CMake):
cmake -S V2/Firmware/simulator/tests/bench -B /tmp/restart-host -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/restart-host -j4
ctest --test-dir /tmp/restart-host --output-on-failure
CCACHE_DISABLE=1 cmake --build /tmp/restart-host --target independent_schedule_acceptance
python3 V2/Firmware/simulator/launch.py --restart-check --capture /tmp/restart
python3 V2/Firmware/simulator/tests/check_restart_render.py /tmp/restart
python3 V2/Firmware/simulator/launch.py --calculated-check --capture /tmp/calculated
python3 V2/Firmware/simulator/tests/check_calculated_render.py /tmp/calculated

These are PC calculations and synthetic native_sim captures. No target compilation,
flash durability/power-loss/RTC behavior or physical board execution is established.
See ostc4_baseline/CHECKPOINT.txt for format, recovery and future adapter evidence.
