Ticket03: freediving settings and bottom field; PC/native simulator only.
No Nordic/physical-watch/sensor or real-diving validation. Process persistence
is deferred to ticket04; these preferences survive sessions in one process.

Integration includes ticket02 Statistics (fff61a6), no duplicate record tracker.
Settings and Bottom Field reuse scuba's clipped 155px scrolling viewport,
selected-row anchor and capsule with 250ms ease-in/out animations. Water uses
scuba's existing Fresh/Marine icon-row positions and fitted capsule.

Reproduction from V2/Firmware/simulator:
  python3 launch.py --apnea-settings-check --gallery /tmp/apnea-settings
  python3 tests/check_apnea_settings.py /tmp/apnea-settings
  python3 launch.py --apnea-check --gallery /tmp/apnea-main
  python3 tests/check_apnea_main.py /tmp/apnea-main
  python3 tests/check_apnea_stats.py /tmp/apnea-main
  python3 launch.py --gallery /tmp/apnea-ui
  python3 tests/check_selection.py /tmp/apnea-ui
  python3 tests/check_transition.py /tmp/apnea-ui
  python3 tests/check_compass_sleep.py /tmp/apnea-ui
  python3 tests/check_sleep_ring.py /tmp/apnea-ui
  python3 launch.py --scuba-check --gallery /tmp/apnea-scuba

Final native runs all PASS after integration merge, using direct built binary
with equivalent scenario/gallery variables and --no-rt. Settings journey has
50 stages, drives public crown/single/double input and depth/timestamp controls,
and observes public rendered labels and scuba accessors. It checks all three
bottom choices in immersion/surface, placeholders, live 12.3m depth record,
1:10 duration record before surfacing (1:11 after), cancel, bounded endpoints,
Marine then Fresh confirmations, water-editor-open-before-descent and bottom-
editor-open-before-descent rejection, disabled underwater entries, continuing
session/recovery state, and fresh session reentry preserving applied choices.
Scuba water/bottom/ring and stored summary are unchanged across the journey.

Meaningful red: with only apnea_main.c reverted to the ticket01 version, the
new public journey fails Missing SESSION APNEA at step3. This is saved as
bottom-display-baseline-red.txt. Restoring new field presentation makes it pass.
Earlier trial fixture failures (overlapping harnesses, outgoing pages counted
as visible, row reparenting x offset) were diagnosed and fixed; the final public
observer uses actual LVGL clipped visibility and checks label horizontal bounds.
Water capsule geometry is fitted to the inherited scuba icon layout; fixed
centre assertions apply to the scrolling Settings/Bottom Field lists.

Public rendered checks PASS at x1 and x2: bounded centred carousel capsule,
fitted water capsule, absent red/green recovery ring on Settings. PNG captures
include selected session depth, applied Marine, underwater water editor, bottom
editor at both scales, and reentry placeholder. Full Main/Statistics rendering,
selection, page transitions, compass/sleep and perimeter checks also PASS.

The legacy full gallery now enters Water through Settings, browses Marine,
confirms, returns and exercises existing Leave. The water animation fixture
likewise exercises actual pending editor selection, rather than hidden V1 UI.

Unchanged baseline failures from ticket01 remain explicitly preserved there:
check_scuba_render.py missing decompression indicator;
check_watch_faces.py selected clock face retention. No assertion was weakened.
