Missing-data duration and elapsed-time axis — user followup, 2026-10-28.

Sensor loss starts a known ongoing synthetic interruption. LOST m:ss advances with
simulation speed and freezes on Pause; physiology and pressure samples stay at the
last accepted input. Known completed recovery gaps show GAP m:ss. Unknown clocks
or unbounded history show GAP UNKNOWN. Missing data is visible in preparation too.
Time-axis ticks are min:sec; the red marker follows elapsed time while the cyan
pressure trace stops. Knowing the duration never establishes pressure exposure.

Frames0..6 repeat valid recovery, completed5:00 gap, unknown time, corruption,
incompatible schema, stale checkpoint and explicit fresh initialization. Frame7:
ongoing2:05 signal loss, elapsed39:05 versus last accepted sample37:00. Frame8:
timestamp-bounded replay gap66:40, no accepted exposure after initial state.
16/16 Release headless checks pass, including pause/speed, repeated loss, reset,
unknown clock and bounded replay. Native restart/calculated/bench recipes pass;
gap-time, restart, calculated and bench render checks pass. C warnings are errors;
assertions stay active in Release. See logs and manifest for exact source identity.

Reproduce from repository root:
cmake -S V2/Firmware/simulator/tests/bench -B /tmp/gap-host -DCMAKE_BUILD_TYPE=Release
CCACHE_DISABLE=1 cmake --build /tmp/gap-host -j4
ctest --test-dir /tmp/gap-host --output-on-failure
python3 V2/Firmware/simulator/launch.py --restart-check --capture /tmp/gap-time
python3 V2/Firmware/simulator/tests/check_gap_time_render.py /tmp/gap-time
python3 V2/Firmware/simulator/tests/check_restart_render.py /tmp/gap-time

Synthetic PC display/evidence only. No target build or physical sensor/RTC tests.
Portable engine/checkpoint arithmetic and numerical tolerances are unchanged.
