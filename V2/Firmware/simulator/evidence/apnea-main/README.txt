Ticket 01: freediving Main and session lifecycle. PC/native simulator evidence only.
No physical watch, Nordic target, real sensor or diving-safety validation.
Statistics redesign and Settings/persistent bottom-field preferences remain deferred.

Reproduce from V2/Firmware/simulator:
  cc -std=c11 -Wall -Wextra -Werror -Isrc src/sim_dive.c tests/test_sim_dive.c -o /tmp/test-apnea
  /tmp/test-apnea
  python3 launch.py --apnea-check --gallery /tmp/apnea-main-gallery
  python3 tests/check_apnea_main.py /tmp/apnea-main-gallery
  python3 launch.py --dive-check --gallery /tmp/apnea-ring-gallery
  python3 tests/check_recovery_ring.py /tmp/apnea-ring-gallery

Red/green:
- Before tracker implementation, the live-record assertion max_dive_s == 5 failed.
- Before session API implementation, the public lifecycle journey failed compilation
  because the session transition and observable phase/current-maximum outputs did not exist.
- check_apnea_main.py BASELINE --baseline fails at the missing cyan separator on
  30-underwater.bmp from unchanged a483e0c. New native captures pass.
- Final portable tracker, native Main journey and pixel/geometry checks all PASS.

The controlled native journey drives public watch crown inputs, depth controls,
and timestamped tracker samples through the same production Main/update loop.
It checks strict 1.0/1.1m start and 0.3/0.2m end, count-at-start, current/last/session
separation, live records, descent/hold/ascent and shallow surface fluctuations,
a 60s apnea and recovery at 119/120/130/661s, page continuity, second apnea,
Leave/re-entry reset and exclusion of time outside the mode. Main is captured at
x1 and x2 with 100.0m and 10:00/10:01. The existing public depth control caps at
100.0m; no fixture bypasses that cap. Portable data tests cover independent values.

Regression passes:
- Native full UI/input/page transition gallery (58s), native scuba interaction
  gallery (44s), and native freediving recovery gallery (23s).
- Recovery-ring, selection, page-transition, compass/sleep and sleep-ring pixel checks.
- Portable scuba model and scuba presentation tests.

Unchanged baseline failures, preserved without weakening assertions:
- check_scuba_render.py line 34: Missing decompression indicator (expects orange
  DECO caption while current scuba presentation uses white).
- check_watch_faces.py line 75: Selected clock face was not retained.
Both failures were reproduced on untouched integration baseline a483e0c and on
this branch. Baseline galleries: /tmp/apnee-baseline-scuba and /tmp/apnee-baseline-ui.
New Main checks do not claim those scripts pass.

Implementation keeps the scuba model/continuous exposure adapter independent:
only sim_dive and freediving presentation fields reset on session entry. No
physiological engine, exposure history, recording/GF or saved-data reset was added.
