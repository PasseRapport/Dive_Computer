Ticket17 implementation evidence — PC only

Base: 2d2c0646457181fbb176d85f0aa166d04554f257
Pinned source revision: 7198d203520f3a3cdccf1ccc0bf8af6475ee4819 (unchanged).

TDD vertical slices:
01-red/green.log: public bench observation, valid equilibrium zero vs invalid.
02-red/green.log: calculated summary observation/availability mapping.
03-red/green.log: elapsed surface time after manual recording completion.
04-red/green.log: approved French formatting, zero/1/59/60/61/754/>24h.
05-red/green.log + JSON: independent public numerical expectation comparison
(initial red lacks public serialized estimate; green exact 27 checkpoints).
06-red/green.log: actual native summary label missing, then all exact strings,
acute glyph, six retained fields, bounds inside circular aperture, live minute
updates, pause/reopening, immediate validity loss, persistent zero and long hours.

Independent fixtures were authored separately before production comparison and
are preserved in simulator/tests/bench/desaturation-reference, including frozen
expected.json, source provenance, inputs, formula implementation and explicit
integer-boundary analysis. No tolerance was added. 05-green.json retains each
raw observation and each expected/actual/difference integer-minute comparison.

Operational-invariance.json compares 578 public observations and 49708 preexisting
scalar values exactly to base, with zero differences. Baseline and current JSONL
and the comparison script are saved here. Existing GF reference limits remain
0.006 m for endpoints and three seconds only for total TTS; unrelated outputs
retain their existing policies.

bench-regression.log: 27/27 tests, including core identity/numerical/recovery and
controller/calculated-watch integration plus independent desaturation.
firmware-host.log: 14/14 host firmware/core tests (run by parent).
calculated-render.log and calculated-render-check.log: native journey and all 10
saved calculated/graph states pass, including two dives, auto ascent and alerts.
gf-render.log and gf-render-check.log: native GF lock/editor and paired ceilings,
mandatory stop/countdown, alerts and explicit invalidity pass.
render-check.log: 13 summary captures pass native geometry/text plus saved pixels.
scripted-render.log: native scripted scuba scenario passes; old pixel checker
scripted-render-check.log reports Missing decompression indicator in the untouched
main-dashboard scuba-13-deco capture. Investigation remains explicit; no threshold
was changed. scripted-summary.png preserves the unavailable summary presentation.

render/summary-0..7: real accepted pressure/timestamp exposure, loaded 1343,
paused 1343, surface 1342, reopened 1340, invalid placeholder, zero then persistent
zero, and 3517 minutes (58 h 37 min). Each has full PNG/JSON/CSV and cropped watch PNG.
render/summary-8..12: presentation-only fixtures 1/59/60/61/754 supplied through the
existing calculated-watch adapter. Their real controller result stays 3517; the
summary observation is deliberately replaced for formatting acceptance only.
These are not independent physiological profiles or fabricated normal UI data.
Native assertions establish exact strings, a real acute glyph, no overlap and
all label corners inside the circular aperture. watch-grid.png shows live states.
Native printk truncates UTF-8 diagnostic labels in the console log; assertions,
rendered glyphs, images and JSON remain authoritative.

lifecycle-rejected-assumption.log is preserved: a strengthened test initially
assumed compartment 6 after a longer second surface interval must exceed its
value shortly after dive 1. That assumption is false. The corrected test checks
residual loading above fresh equilibrium before/after dive 2, summary mapping,
repeated read nonmutation, pause and exact 1x/10x/60x replay equivalence. It passes
in lifecycle.log and the final full bench log. No production formula changed.

Host/native compilation and execution only. No target display port, target UI,
physical board, timing, stack high-water, certification or safe-to-dive claim.

Review fixes (following commit 6b722d0):
- A corrupt/rejected checkpoint retained old diagnostic numbers and marked older
  fields invalid, but omitted the new desaturation availability. Added it to the
  same invalidation cascade. Repeated public rejection now exposes INVALID;
  tracked time/tissues and retained diagnostic minutes are unchanged, while the
  summary immediately presents the unavailable placeholder.
- review-fix-red.log reproduces failures through the public rejection/summary
  seam and existing disk-recovery fault suite. review-fix-green.log records both
  passing after the minimal fix. review-fix-bench-regression.log records the full
  27-test suite after correction.
- Audited other invalidation paths: missing exposure and ordinary restart feed
  the existing derived ostc4_read observation (desaturation follows invalid
  tissues); rejected predictions explicitly mark desaturation invalid. The
  retained-observation overwrite in checkpoint rejection was the missing path.
- Bundled the exact source Montserrat copyright/OFL license at
  simulator/licenses/Montserrat-OFL.txt and preserved its notice/provenance in
  the generated acute-glyph asset and generator. review-font-license.json records
  source/font/license hashes, exact license-copy integrity, identical output
  across two regenerations and unchanged glyph payload. No visual change.
