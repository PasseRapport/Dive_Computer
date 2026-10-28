OSTC4 ticket06 - synthetic PC evidence only

Reproduce from repository root:
  cmake -S V2/Firmware/simulator/tests/bench -B /tmp/watch-bench -DCMAKE_BUILD_TYPE=Release
  CCACHE_DISABLE=1 cmake --build /tmp/watch-bench -j4
  ctest --test-dir /tmp/watch-bench --output-on-failure
  python3 V2/Firmware/simulator/launch.py --calculated-check --capture /tmp/calculated
  python3 V2/Firmware/simulator/tests/check_calculated_render.py /tmp/calculated
Published PNG captures can be checked with prefix evidence/ticket06/calculated.

Captures0..9: unavailable; shallow NDL with qualified zero-TTS footer;
30m/35min deco; auto at deepest calculated12m stop; manual ceiling violation;
acknowledged violation with obligation retained; explicit missing-exposure
invalidation; surface interval; second dive; between true ceiling and stop.
Each JSON contains current raw result/watch state and each CSV contains exact
accepted pressure/time inputs on the same1s grid. pc_bench_checks.c supplies
the control recipe, including explicit missing and acknowledge commands.
The preceding CSV alone does not recreate a missing-exposure report.

Renderer, session, alert and automatic behavior are integration evidence, not
an independent numerical oracle. The independent v4 comparison accepts all6
fixed profiles with the explicitly user-approved absolute TTS tolerance3s;
NDL, stop slots and validity remain exact. Raw+1s differences and prior strict
rejection are retained in ostc4_baseline evidence. No target build or physical
execution is claimed. Suspended deco_core/BMAD work was not changed.
