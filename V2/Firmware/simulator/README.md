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
