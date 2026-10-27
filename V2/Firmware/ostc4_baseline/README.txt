OSTC4 experimental unchanged PC baseline (ticket 01)
=================================================
One standalone portable-C calculation adapter and a headless replay tool.
No integration with Zephyr, LVGL, watch drivers or scripted demonstrations.

From the repository root:
  cmake -S V2/Firmware/ostc4_baseline -B /tmp/ostc4-baseline-build \
    -DCMAKE_C_COMPILER=/usr/bin/gcc -DCMAKE_BUILD_TYPE=Release
  cmake --build /tmp/ostc4-baseline-build -j
  ctest --test-dir /tmp/ostc4-baseline-build --output-on-failure
  /tmp/ostc4-baseline-build/ostc4_replay \
    V2/Firmware/ostc4_baseline/fixtures/shallow.csv
  python3 V2/Firmware/ostc4_baseline/record_evidence.py \
    /tmp/ostc4-baseline-build /tmp/ostc4-observations

Build dependencies: C11 compiler (tested GCC on Linux), CMake, libm and an
ELF linker supporting function/data sections and --gc-sections. Python 3
standard library is used only for tests and evidence recording. No network
fetch is required. Upstream headers contain a zero-length array extension;
the original bytes are retained, so this build currently needs GCC/Clang.
The public header has no platform dependency. Test assertions remain active
in Release builds. Adapter/replay code compiles with warnings as errors.

Source and notices:
  upstream/                 11 exact original files; do not edit
  upstream-manifest.json    exact revision, selected file hashes, archive hashes
  COPYING.html              GPLv3 text retained from the completed reuse audit
  compat/                   declaration-only RTC types; no HAL implementation
  include/, src/            new public interface and adapter/CLI, GPL-3.0-or-later
  fixtures/                 timestamped synthetic inputs (not dive schedules)
  policy.json               declared extraction policy and original coefficient table
  evidence/                 baseline observations and actual compiler/link inputs

The two original translation units are compiled unchanged. Their original
headers are private dependencies of the calculation adapter. The linker
removes unrelated helpers (including the gas-list helper referencing calc_MOD).
No fake calc_MOD, hardware, settings-service or clock implementation is supplied.
The retained original firmwareEraseProgram.h is a declaration-only include
needed by settings.h; no flash management code is compiled or linked.

Upstream GPL-3.0-or-later notices are preserved verbatim in the selected files.
buehlmann.c itself has no full license notice; its companion buehlmann.h does.
Compatibility shims, adapter, replay and support scripts added for this ticket
are GPL-3.0-or-later. Manufacturer provenance and distribution adoption remain
separate gates; this fork beta is not identified as a stable manufacturer build.

Public input/output contract:
  ostc4_initialize explicitly starts one fresh synthetic surface-equilibrium
  scenario. It is never an automatic history/restart recovery action.
  ostc4_advance consumes a timestamp and absolute bar. Linear interpolation
  between pressures is declared input behavior, not inferred sensor exposure.
  Equal pressures use original cached exposure factors, including known defects.
  ostc4_read copies the most recent accepted observation, without calculation.
  There is one active scenario and calls must be serialized. No thread safety,
  independent prediction API, checkpoint or repetitive-dive lifecycle is claimed.

CSV header is timestamp_seconds,absolute_pressure_bar. The first data row
explicitly declares initial timestamp and surface pressure (0.8..1.2 bar).
Later timestamps must be unsigned integral seconds, strictly increasing, no
more than 3600 s apart. No implicit subsecond rounding. Pressures must be
finite and between the declared surface and surface+6 bar. Bounds are this
experimental replay's supported input envelope, not a real diving limit.
There is no unknown-gap marker: every accepted interval declares a complete
linear segment; callers must not submit incomplete measured history as one.
Input errors reject the sample transactionally. A read after rejection still
returns the prior accepted timestamp; never treat it as a result for the bad
sample. The CLI exits nonzero on any rejection; preceding JSONL is incomplete.

All nitrogen/helium arrays, raw NDL/TTS seconds, raw ceiling metres, 50 raw
stop durations, anchor values and per-result availability are observable.
Stop index i corresponds to gauge depth (i+1)*3 m at the declared 0.1 bar/m.
The dive timer counts all intervals with either endpoint below surface,
excluding wholly surface intervals. Fresh reset sets the settings anchor and
upstream GF-low global to zero. Each accepted advance calls planner followed
by ceiling and retains upstream settings-anchor/global-GF changes. Read calls
cannot change those values. Prediction isolation is ticket 03 work.

Unavailable: before 60 accumulated underwater/transition seconds, at surface,
or ambiguous all-zero NDL/TTS output. Limited: NDL reaches 14400 s or TTS / a
stop reaches 59940 s; saturated TTS also makes ceiling/stops limited.
Available means only that upstream produced an estimate. It does not establish
numerical correctness. No-deco upstream TTS=0 is preserved and explicitly means
"upstream zero", not ascent travel time. Raw values are retained even when
the availability wrapper marks them unavailable. Errors are separate status
returns and must never be presented as a valid zero.

The air/GF/ascent/stop/NDL policy, all 16 N2 half-times, coefficients, time
constants and known defects are recorded in policy.json. Precision, unsuffixed
constants, double math calls and original evaluation order are unchanged.
The build requests -O2 -fno-fast-math -ffp-contract=off; reproduction.json
rejects unrelated build directories, rebuilds the replay target, verifies its
compiled-in identity and records compiler commands, link commands (Make/Ninja),
host libc information, adapter-source hashes and executable/input/output
hashes, exact source manifest and policy. JSONL floats use 9 significant digits
to round-trip binary32 observations. Replay requires round-to-nearest.

Evidence levels:
  PC: headless compilation, public contracts and repeatable synthetic replay.
  Independent numerical schedule acceptance: not established (ticket 02/05).
  Target build for this extraction: not performed.
  Physical execution, timing, stack and real-dive fitness: not established.
The earlier audit's isolated Nordic feasibility evidence is not this ticket's
full-watch validation. Existing BMAD engine/story 1.6 and simulator remain intact.
