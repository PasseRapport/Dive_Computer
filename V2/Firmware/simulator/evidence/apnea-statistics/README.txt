Ticket02: session Statistics. PC/native simulator evidence only.
No Nordic target, physical watch or real-dive validation.

Reproduce from V2/Firmware/simulator:
  python3 launch.py --apnea-check --gallery /tmp/apnea-statistics
  python3 tests/check_apnea_main.py /tmp/apnea-statistics
  python3 tests/check_apnea_stats.py /tmp/apnea-statistics

Red/green: before replacing Statistics, check_apnea_stats.py against the unchanged
7e55172 capture apnea-14-statistics-no-ring.bmp fails: Statistics lacks common
orange separator (red-baseline.txt). After implementation, the public crown,
depth and timestamp journey and native pixel checks pass. Native sampling avoids
resizing/interpolating x2 colors; one-pixel antialias edge tolerance only applies
to comparisons of the cyan/orange separator shapes, not their field positions.

Statistics consumes watch_data session records directly. Captures cover initial
placeholders/zero count/running session, new records during an active apnea,
recovery without ring/direction, a second shorter/shallower apnea retaining the
previous records, new live records in a third apnea and fresh re-entry.
Elapsed session includes both surface and immersion. The session footer uses
H:MM:SS, separate from the longest-apnea M:SS value.
At x1: 100.0m, longest1:00, session0:01:31.
At x2: 100.0m, longest101:00, session1:41:31.
Long-duration fixture: longest1666:40, session27:47:11. These are synthetic
layout scenarios, not claims about realistic apnea duration.
Main recovery presentation/ring is checked again after returning from Statistics.
Orange #FFA438 rails use the identical shared geometry as cyan #33DBD4 Main.
Battery header remains visible on both data pages. Statistics has no direction
widget and the controller hides the Main recovery arc on this page.

Regression results:
- Main native journey and check_apnea_main.py PASS.
- Full native UI/input/page-transition gallery PASS.
- Native scuba interaction/scenario/selector gallery PASS.
- Selection and transition pixel checks PASS.
- check_scuba_render.py retains its unchanged pre-existing failure at line34,
  Missing decompression indicator. This expects orange DECO text while the
  baseline currently uses white. No assertion was weakened or changed.
The separate unchanged baseline watch-face pixel failure documented by ticket01
remains outside this ticket; no watch-face or scuba calculations were changed.

Settings and durable preferences are separate tickets and are not claimed here.
