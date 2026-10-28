Battery: hover the visible battery button and scroll up/down by one percent, bounded at 0 and 100. Changes and low-battery state apply while paused. Battery wheel events do not navigate the watch. Three-value click cycling removed.
NO DATA moved down five pixels. Red rectangle unchanged at (52,146), 94x58; rendered white glyph margins are 9 pixels above and below.
Validation: calculated native recipe passes real SDL wheel events at 87->88->87->100->0->1->87, paused time/page isolation, low-battery warning and recovery. Calculated, gap-time and restart render checks pass. PC/native evidence only.

Source SHA256:
2bc7b0cbf549044d7e778279e11138f272380fa45d8ffca38b4891622e490fdf pc_bench_checks.c
aa1b49544e6925d6f8c8a003cd3f588b7c0b3048d157e90d0557b17df434ff95 scuba_checks.c
1aa1f5285243c4d0c86529e722fbffa7f78091e0d3d85fe076c4b26411f25a65 scuba_dashboard.c
3bc28f8d515ecac68e082fb99c94615ad48668433223529a21c6dc52ef4daada scuba_ui.c
23c751932853d215bb80e56754dbbdbf6459aee1ee5705a4883b1185c562248a sim_controls.c
a1280992c335d518c72238889ce1b7878f526005692a59781c8b826770326485 sim_host.c

Standalone scripted scuba native recipe also passes with real battery wheel events (87 -> 15 -> 5).
