# Dive Watch LVGL simulator

Desktop watch UI built with LVGL and Zephyr `native_sim`, using the Nordic SDK.
This replaces the former standalone SDL/scanline simulator at this location.

The simulator starts at **Scale ×1**. The Scale button switches between ×1 and ×2.
The dive battery stays at the top; compass mode lowers it slightly so perimeter labels pass above it. The north tick is red. Dive depth grows leftward from a decimal point centred horizontally on the screen.

## Launch

From the repository root:

```sh
python3 V2/Firmware/simulator/launch.py
```

Or from this directory:

```sh
python3 launch.py
```

The launcher builds the application in a temporary directory, then opens the
window. Sources stay here; generated builds and caches stay outside the repo.
Close the window or press Ctrl+C in the launching terminal to stop it.

Prerequisites: Python 3, a C compiler, SDL2 development libraries and the Nordic
nRF Connect SDK 3.4.1 with its toolchain. Defaults match the local installation:
`~/ncs/v3.4.1` and `~/ncs/toolchains/8285d8ad56`. Override them as needed:

```sh
DIVE_NCS_ROOT=/path/to/ncs/v3.4.1 \
DIVE_TOOLCHAIN_ROOT=/path/to/toolchain \
python3 launch.py
```

The SDK supplies Zephyr and LVGL. No LVGL sources need to be copied into the repo.
This is a desktop prototype; integration with the Nordic board is separate.

## Controls and screens

- Hover **CROWN** and scroll to rotate the crown. Menus stop at either end.
- **Single tap** selects or confirms; **Double tap** returns where supported.
- **Scale** switches between x1 (one display pixel per window pixel) and x2.
- Hover **ORIENTATION** and scroll to rotate the compass in 5-degree steps.
- **Ascend −1** and **Descend +1** adjust vertical speed by 1 m/min per click, between −60 and +60 m/min; zero holds depth. **Surface** resets depth and speed.

The simulated matrix is 320 x 300, with a 300-pixel circular aperture. Grey shows
what is outside the aperture. It is a UI preview, not a hardware geometry test.

Startup shows the octopus logo, then the digital or analog watch face. Turn the
crown and fill the exit ring to reach **Watch / Diving / Compass / Options**.
Options contains **Bluetooth / Watch / Backlight / Haptics / Sleep**. Bluetooth
and haptics are previews; they do not drive physical hardware.

**Freediving** retains the V1 screens, water selection and recovery ring.
**Scuba Diving** has preparation/settings, depth with its metre unit, dive time,
NDL, display customization, stop displays, alerts and the last completed summary.
The in-dive dashboard places depth at the top, ascent chevrons at the centre,
NDL/decompression/stop information at the left and dive time at the right.
Top-level scuba pages are **Live / Options / Leave**, navigated with the crown.
**Leave** is unlocked only at the surface, to the right of Options. Options is
an entry page: tap to open **Ring / Bottom field / Backlight / Back** directly.
The Options list, Ring and Bottom field keep the selection centred while their
text scrolls with the same animation. Backlight shares the watch settings:
**ON–OFF–AUTO / Brightness / Sensitivity**. Tap the mode to cycle, or open a
value editor, turn the crown and tap to confirm. Double tap backs out of menus.
The live double-tap brightness shortcut remains available.

Ring choices are **Auto / No deco / TTS / Stop duration / Compass**. Bottom-field choices
are **Temperature / Max depth / Clock / Battery / Safety stop / TTS / ppO2 /
Heading**. Their selection capsule stays at the centre while the list animates
vertically over 250 ms, without wrapping. Tap to apply and return to Options.
The NDL ring spans 0–60 minutes and turns orange strictly below five minutes.
TTS uses a fixed orange 0–50 minute scale. Stop rings show the remaining time
for the current stop, from full to empty. Auto adapts to dive phases. Stop/decompression transitions
still foreground their lower-field information; the normal chosen field returns
afterwards. Alerts retain tap priority. The standalone compass and freediving
screens are unchanged.

It starts at 2 m by default (Dive Settings allows 0.5–10 m in 0.5 m steps), considers depth below 0.5 m to be the surface, and finishes
after five surface minutes. The exit ring is available at the surface; descending
again before the dive ends continues the same dive. A double tap during a dive
opens brightness; turn the crown and tap to confirm.

Scuba's test panel cycles **Manual / No deco / Safety stop / Deco / Alerts**,
sets simulation speed to **x1 / x10 / x60**, and cycles battery states. Selecting
a scenario resets the current demonstration; using the depth buttons switches
to manual control. Time acceleration affects the scuba simulation only.
**NDL and decompression stops are scripted demonstration values, not a real
physiological model or guidance for diving.**

## Checks

```sh
python3 launch.py --build-only
python3 launch.py --check-controls
python3 launch.py --gallery /tmp/watch-gallery
python3 launch.py --scuba-check --gallery /tmp/scuba-gallery
python3 launch.py --dive-check --gallery /tmp/freedive-gallery
python3 launch.py --water-check
```

`--gallery` runs the native acceptance scenario and saves BMP frames. Rendered
checks require Pillow:

```sh
python3 tests/check_scuba_render.py /tmp/scuba-gallery
python3 tests/check_selection.py /tmp/watch-gallery
python3 tests/check_watch_faces.py /tmp/watch-gallery
python3 tests/check_compass_sleep.py /tmp/watch-gallery
python3 tests/check_sleep_ring.py /tmp/watch-gallery
python3 tests/check_transition.py /tmp/watch-gallery
```

The model tests do not need Zephyr or LVGL:

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc src/scuba_model.c tests/test_scuba_model.c -o /tmp/test-scuba
/tmp/test-scuba
cc -std=c11 -Wall -Wextra -Werror -Isrc src/sim_dive.c tests/test_sim_dive.c -o /tmp/test-freedive
/tmp/test-freedive
```

## Source layout

- `src/watch_ui.c`: watch navigation, options, startup and sleep transitions.
- `src/v1_ui_layout.c`: adapted freediving screens.
- `src/scuba_ui.c` and `src/scuba_model.c`: scuba screens and scripted scenarios.
- `src/compass_ui.c`: standalone watch compass face.
- `src/sim_controls.c` and `src/sim_host.c`: desktop controls and SDL adapter.
- `src/assets/`: fonts, icons and project logo; `tools/` regenerates assets.
- `tests/`, `src/sim_checks.c` and `src/scuba_checks.c`: model, input and render checks.

The parent firmware's `CMakeLists.txt` remains the independent MCU application.

Scuba Options double tap returns directly to the live dashboard. Dive Settings
includes Back. All battery icons use a proportional fill with red at 0–10%,
yellow at 11–50%, and white above 50%. Digital watch: battery above centred
time, date below and octopus at the bottom. Analog watch: white outlined hands,
octopus above centre, day/weekday and battery below. Local PC date and seconds
are used in normal runs; galleries use fixed values for reproducibility.

Focused checks after building (create the gallery directory first):
```sh
DIVE_SIM_SCALE_CHECK=1 <build>/zephyr/zephyr.exe --stop_at=5
DIVE_SIM_APPEARANCE_CHECK=1 DIVE_SIM_GALLERY=/tmp/appearance SDL_VIDEODRIVER=dummy <build>/zephyr/zephyr.exe --stop_at=5
python3 tests/check_appearance.py /tmp/appearance
```
Scale checks inject actual SDL clicks and assert the resulting window size.
The initial reported Scale failure was not reproduced before hardening the
scale state and synchronizing the labels in both desktop panels.

The native simulator reserves 1 MiB for LVGL objects and temporary animation
layers. This is a desktop-only configuration, not the embedded watch RAM budget.
Watch faces have no seconds hand; the analog day/date sits at 3 o'clock.
Brightness and sensitivity share the same blue-bar editor in both contexts.
Ring, Bottom field and Backlight menus provide an explicit Back item.
The optional Compass ring rotates its perimeter scale around the live dive data.

Scale redraw regression: `python3 launch.py --scale-check` runs 60 actual SDL
Scale clicks and verifies both window dimensions and a visible rendered frame.
Native resize/expose events request a full redraw on the UI thread, after input
callbacks finish, so a late window-surface reset cannot leave the simulator black.

Inactive menu rows are gray; headings and the selected row in its blue capsule are white.
Sensitivity has ten percentage intervals, adjusted by one crown detent each.
Battery shells and all progress-ring ends are rounded. Dive Summary shows maximum
depth, average depth, duration, and the live elapsed surface interval.

Ascent estimation preserves millimetre depth samples through the simulator adapter;
rounding to decimetres happens for the visible depth rather than before velocity
is calculated. Constant-rate coverage includes every integer rate from −60 to
+60 m/min at simulated time factors 1, 10 and 60.

The ascent bar has no arrow during descent or below 1 m/min. From 1 to below
2 m/min it shows one gray arrow; at 2/4/6/8/10 m/min it shows 1/2/3/4/5
levels. Arrows are green from 2 through 8, yellow above 8 through 10, and red
above 10 m/min. After five sustained seconds above 10, arrows become white
on a red background. Unused levels remain gray dashes.

Acknowledging a dive notification hides its message completely; active model
conditions and the dive data remain available. During a stop, depth is shown
on black with paired triangles pointing towards each other. Dive-time minutes use a fixed right edge at the units digit; extra digits
grow leftwards, leaving room for seconds inside the round display. Ceiling labels and depth
are enlarged. The alert reads **DECOMPRESSION**; acknowledgement uses a tap without an on-screen hint.

The scuba preparation screen recaps air, water type, battery, start depth and both alarm thresholds (OFF when disabled). Dive Settings opens a dedicated Fresh/Marine selector with the same water icons and animated capsule as freediving; tap confirms and Back cancels.


## Experimental calculated PC bench (OSTC4 tickets 04–06)

```sh
python3 V2/Firmware/simulator/launch.py --bench
```

This expands the existing native simulator to 1200 x 800, adding calculated
OSTC4 diagnostics alongside the watch and a synchronized depth graph below it.
The virtual watch uses the same calculated observation as diagnostics in bench
mode; no scripted fallback is allowed. Without `--bench`, legacy scripted scuba
and apnea demonstrations remain available. Use the lower **bench** row for
calculated exposure; its separate **Auto ascent** button enables automatic
calculated ascent. Watch crown/menu/scenario actions do not initialize tissues;
scripted scenario/speed commands are ignored while the watch is calculated.

The default synthetic replay is the accepted two-dive example: absolute pressure
1.01325 bar at 0 s, 4.01325 at 120 and 1920 s, 1.01325 at 2100 and 3600 s,
3.01325 at 3720 and 4920 s, and surface again at 5040 s. Only **FRESH replay**
(or process startup) initializes equilibrium tissues. Surfacing, holding depth,
pausing, speed changes, manual takeover and normal menus preserve exposure.
At replay completion the clock stops; **Hold**, a depth/rate button or **Surface**
continues manual exposure from the retained state.

Replay interpolates absolute pressure on a fixed one-second input grid, including
original keyframes exactly. Play at x1, x10 or x60 using the same grid and planner
calls: the declared comparison policy is byte-identical full observations within
the same build. This grid differs from the sparse endpoint grid of ticket 03;
its intermediate planner/GF-anchor calls can change schedules. Grid equivalence
is not claimed across those two different input sequences. Pause discards elapsed
host time and preserves the fractional simulation-second remainder. Bench commands
apply at the last completed second; manual changes discard that remainder.

Manual depth uses `absolute_bar = surface_bar + density * 9.80665 * depth_m / 100000`,
with surface pressure shown explicitly and water density 1025 kg/m3 by default.
No display rounding feeds calculations. Depth commands hold or change depth;
rate buttons change signed m/min, positive downward. Inputs are limited to 60 m,
60 m/min and the engine's +6 bar envelope. Initial surface pressure is limited
to 0.8..1.2 bar. The standalone controller also accepts densities 990..1040 kg/m3.
The planner's ceiling/stop depths retain the upstream **10 m/bar convention**,
shown separately from the hydrostatic graph. GF30/70 is the initial preference; the preparation editor commits an integer
GF pair atomically and locks it for the complete recorded dive. Ascent 9 m/min
and last stop/increments of 3 m remain fixed test settings. TTS=0 in upstream NDL mode
**excludes ascent travel**, and is explicitly qualified in the diagnostics.
Unavailable, limited and invalid result flags are shown, including for every
nitrogen tissue. Mandatory stops can be scrolled if the full list exceeds its box.
The watch and engine use10m/bar so gauge depth, ceiling and stop labels share
one coordinate convention. The graph and manual controls use hydrostatic metres,
as labelled. Depth displays truncate millimetre samples to0.1m; calculated
ceilings round upward to0.1m, while ceiling alerts compare raw depths. Stop zones
use millimetre resolution to avoid pressure-conversion flicker at exact stops.
Unavailable/invalid calculation states show explicitly with blank calculated
values and an empty grey ring, retaining last known obligations. Limited results
keep capped values on the watch (including the existing>99 NDL display),
without an extra LIMITED label; diagnostics retain the limited state; the automatic diver holds if a required ascent result is limited.
TTS remains raw upstream output. The watch footer always uses the short TTS
label; diagnostics qualify zero-TTS in NDL mode as excluding ascent travel. The optional180s safety stop remains a separate session
policy, is omitted after mandatory deco, and is never included in raw TTS.
Independent profiles pass ticket05 comparison v4 with the user's±3s TTS margin;
all raw+1s differences and the rejected exact-TTS bundle remain published.

Import a deterministic synthetic pressure profile:

```sh
python3 V2/Firmware/simulator/launch.py --bench-profile /absolute/path/profile.csv
```

CSV header: `timestamp_seconds,absolute_pressure_bar`. Start at second zero and
surface pressure. Use strictly increasing integer seconds, finite absolute bar,
2..1024 rows, ending before 21601 s. The first pressure supplies surface pressure;
water density remains 1025 kg/m3. Segments assert complete, linearly changing
exposure. A separation over 3600 s denotes missing exposure: its graph segment is
marked red, no line is interpolated through it, and reaching it latches INVALID
until explicit fresh initialization. **Mark gap** explicitly reports unknown exposure of any duration, latches all
results invalid and marks the current graph position red. This CSV format cannot
encode shorter unknown intervals. Use only complete synthetic profiles with this
importer, or stop and mark a gap when exposure becomes unknown. Invalid CSV is rejected explicitly.

Graph x is elapsed seconds; y is metres increasing downward from 0 to 60. Cyan
is recorded actual exposure; dashed grey is the known replay future, not a
calculated prediction; yellow is the exposure-clock position. Manual takeover
removes the unused replay future and extends actual history. No speculative
projection is added. History capacity is 21601 points; exhaustion latches invalid
rather than discarding exposure. This host allocation is not a firmware budget.

Headless integration and numerical regression checks (no LVGL/Zephyr required):

```sh
cmake -S V2/Firmware/simulator/tests/bench -B /tmp/pc-bench -DCMAKE_C_COMPILER=/usr/bin/gcc
cmake --build /tmp/pc-bench -j4
ctest --test-dir /tmp/pc-bench --output-on-failure
```

Native SDL interaction/render checks, with four paused milestone captures:

```sh
python3 V2/Firmware/simulator/launch.py --bench-check --capture /tmp/bench-visible
python3 V2/Firmware/simulator/tests/check_bench_render.py /tmp/bench-visible
```

Pillow is needed for the pixel checker. `--bench-check` uses native simulated
time without real-time throttling and the same exposure clock. Existing galleries
and legacy interaction checks run separately without `--bench`. The launcher
builds only the contained corrected exposure variant into this bench; comparison
baseline/corrected executables remain headless tools, with no runtime selector.
The source/correction/policy identities are in `../ostc4_baseline` and its prior
evidence bundles. PC success does not establish a Nordic target build, physical
memory/timing/stack fitness or readiness for operational dive guidance.


### Automatic ascent and calculated watch checks

**Auto ascent** explicitly leaves replay and starts from the last accepted
pressure. It commands up to9m/min toward the deepest currently calculated
mandatory stop, otherwise toward the surface. It holds while required results
are unavailable, limited or invalid, or when already above a required stop;
it does not invent a descent recovery. It ignores optional safety stops. Every
accepted second is recorded as actual exposure. A manual depth/rate/hold/surface
command cancels automatic control. Re-engage explicitly with **Auto ascent**;
this discards an unobserved manual target and retains observed tissues, clock,
anchors and history. Pause stays paused. **Auto: ON** toggles off to a hold at
accepted depth. No projected automatic path is presented as completed exposure.

The presentation adapter consumes each completed exposure sample, including
accelerated operation; it owns no physiology and does not call the engine.
Mandatory countdowns are refreshed from current stop calculations, separately
from TTS and the optional safety timer. Acknowledgement changes alert visibility
and haptics only. Menus and recording transitions do not reset exposure.

```sh
python3 V2/Firmware/simulator/launch.py --calculated-check --capture /tmp/calculated
python3 V2/Firmware/simulator/tests/check_calculated_render.py /tmp/calculated
```

This deterministic native recipe captures unavailable output, shallow NDL,
30m/35min decompression, an automatic mandatory stop, manual violation before/after
acknowledgement, invalid exposure, surface interval and second dive. Each capture
has a result/watch JSON and exact accepted pressure/time CSV. Gap captures carry
invalid flags: replaying their preceding known CSV alone cannot recreate the
explicit missing-exposure command. The scenario recipe is in pc_bench_checks.c.
This is synthetic PC evidence; no target or physical execution is implied.

### Configurable GF preparation (synthetic PC only)

In scuba preparation, the committed GF pair appears beside AIR and the water
type. Tap for Dive Settings, choose GF, turn to edit Low in steps of one, tap to
edit High, then tap through the complete-pair confirmation. Double tap cancels.
Both endpoints must satisfy `1 <= Low <= High <= 100`; equality is accepted.
A dive beginning during editing discards the unfinished draft and locks the last
committed pair before the starting exposure calculation. Brief surfacing retains
the lock. Manual surface completion or five surface minutes releases it.

Changing the pair while unlocked recalculates at the same exposure instant;
tissues, elapsed time, exposure history and incoming GF anchor remain intact.
Normal replanning can increase the persistent first-stop anchor. Completion does
not clear it. The PC diagnostic panel and replay JSON expose preferred/applied
pair, recording lock and distinct configurable/legacy policy identities.
`GF-CONFIGURATION.txt` records the policy and independent comparison evidence.

Run the editor/lock demonstration and verify its captures:

```sh
python3 V2/Firmware/simulator/launch.py --gf-check --capture /tmp/gf-demo
python3 V2/Firmware/simulator/tests/check_gf_render.py /tmp/gf-demo
```

Checkpoint v2 carries the pair and lock; in-memory recovery also captures the
recording controller. A later settings/manual recording command makes an older
pressure-only recovery point ineligible, preserving its bytes and reporting the
reason. Save again after configuration changes. The portable core still explicitly rejects old fixed-policy v1 data; the PC disk
adapter below recognizes its pinned policy and exposes its lifecycle ambiguity.
This is PC/synthetic evidence only.


### Disk checkpoint recovery (synthetic PC only)

The lower bench row has separate **Save disk** and **Load disk** controls beside
the original memory/fault controls. Files use `DIVE_WATCH_STATE_DIR/recovery.checkpoint`;
normal interactive startup configures the same state directory as preferences/logs.
Nothing resumes automatically. Saving does not change tissues, active pair or lock.
Writes use a same-directory private temporary file, flush/fsync, then atomic rename;
failed writes preserve the previous file and live exposure. This is a PC filesystem
contract, not a claim about power-loss persistence or hardware flash/RTC behavior.

The disk clock initially reads **Clock UNKNOWN**. Loading in that state reports
unknown restart time and INVALID guidance. Clicking the clock selector explicitly
asserts **saved time; elapsed = 0**: this means the caller has established the exact
saved simulation-clock instant and zero intervening exposure/configuration changes.
It is a deliberate synthetic test assertion, never an elapsed-wall-time inference.
Load consumes and clears that assertion. Real later-time recovery must instead
supply complete evidence through `pc_bench_checkpoint_file_load(path, evidence,
configuration_unchanged)`: known monotonic restart time, age limit, every accepted
linear pressure/time segment since save, and an explicit assertion that no GF,
manual recording or start-threshold configuration events occurred. Pressure history
alone cannot prove those configuration events. Unknown clock, gaps, stale data,
corruption, incompatible policy/schema and invalid settings invalidate guidance;
accepted diagnostic numbers, pair/lock and prior files remain available.

The 364-byte DWCP schema-1 PC envelope contains a fixed 64-byte header, the exact
296-byte portable EXC1 schema-2 checkpoint, and little-endian CRC-32 of the full
preceding envelope. Header fields are magic[0:4], schema u32@4, size u32@8,
density binary32@12, preferred Low/High u8@16/17, recording armed/active u8@18/19,
start threshold u32@20, start/completion/dive/surface seconds u32@24/28/32/36,
started/completed counts u32@40/44, and zero reserved bytes48..63. The core
checkpoint owns numerical policy, tissues, exposure time, applied GF/lock and
persistent planner anchors; its representation is not duplicated in the envelope.
CRC detects accidental corruption, not authenticity. Restore verifies policy,
integrity, lifecycle/lock consistency and continuity before exposing valid results.
A recovered active pair overrides next-dive preferences until normal completion;
brief surface returns retain the lock. Recovered history starts at the checkpoint:
no earlier trace, surface interval or controller state is fabricated.

A recognized original 293-byte fixed-30/70 EXC1 schema-1 file with the exact pinned
numerical-policy fingerprint and CRC is explicitly migrated in memory to30/70.
Its bytes/provenance remain untouched. It lacks recording lifecycle evidence, so
recovery reports **Legacy ... recording lifecycle UNKNOWN**, keeps GF locked and
guidance INVALID, and cannot be saved as a valid new checkpoint. An unrecognized
policy is incompatible. Fresh remains a separate explicit synthetic action.

```sh
python3 V2/Firmware/simulator/launch.py --checkpoint-check --capture /tmp/disk-checkpoint
```

The host tests use separate OS processes for saved-instant and complete later
exposure recovery, compare every public tissue/anchor/NDL/TTS/stop/validity and
recording result exactly against uninterrupted matched-cadence control, and cover
preferred-pair independence, brief surface/manual/automatic completion, immutable
legacy migration, unknown configuration/clock/exposure, corruption and write
failures. Exact continuation equality establishes serialization consistency only;
existing independently authored numerical acceptance remains a separate gate.
