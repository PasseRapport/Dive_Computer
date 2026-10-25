# Dive Watch LVGL simulator

Desktop watch UI built with LVGL and Zephyr `native_sim`, using the Nordic SDK.
This replaces the former standalone SDL/scanline simulator at this location.

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
- **Descend**, **Ascend** and **Surface** control the simulated depth.

The simulated matrix is 320 x 300, with a 300-pixel circular aperture. Grey shows
what is outside the aperture. It is a UI preview, not a hardware geometry test.

Startup shows the octopus logo, then the digital or analog watch face. Turn the
crown and fill the exit ring to reach **Diving / Compass / Watch / Options**.
Options contains **Bluetooth / Watch / Backlight / Haptics / Sleep**. Bluetooth
and haptics are previews; they do not drive physical hardware.

**Freediving** retains the V1 screens, water selection and recovery ring.
**Scuba Diving** has preparation/settings, depth with its metre unit, dive time,
NDL, compass, statistics, stop displays, alerts and the last completed summary.
It starts at 2 m, considers depth below 0.5 m to be the surface, and finishes
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
- `src/compass_ui.c`: compass face shared by watch and scuba views.
- `src/sim_controls.c` and `src/sim_host.c`: desktop controls and SDL adapter.
- `src/assets/`: fonts, icons and project logo; `tools/` regenerates assets.
- `tests/`, `src/sim_checks.c` and `src/scuba_checks.c`: model, input and render checks.

The parent firmware's `CMakeLists.txt` remains the independent MCU application.
