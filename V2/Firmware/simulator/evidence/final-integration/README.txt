Final simulator GF integration evidence — 2026-10-28

Reviewed code commit: 8215527b770c7b0f09f94e0fa9d149dd6d22c818 on codex/simulator-gf.
Whole branch compared with base37d36f68686a732f03f736fe39528bed623fcc31;
review-fix delta compared with a83b5b26f246479979d756c42e9d9572585848c9.
Standards and Spec reviewers independently reproduce prior failures as corrected:
0 unresolved findings on each axis. Initial reports are retained alongside final.

Fresh combined host suite:26/26PASS. Full independent comparison:578observations,
exact NDL/stop slots/validity, zero TTS differences; endpoint max abs4.999294e-6m
under predeclared0.006m bound. Numerical correction evidence and original rejected
stress results remain in sibling numerical-correction and ticket13 directories.

After final shared-layout/recovery corrections, native --checkpoint-check PASS,
check_checkpoint_render.py PASS; native --gf-check PASS, check_gf_render.py PASS,
check_gf_ceilings_render.py PASS. Two final PNG/JSON examples included; gf7 shows
both fixed-GF ceilings and mandatory stop depth, disk2 shows active40/85 locked
with independently preferred20/90 after explicit known-time recovery.

The older restart pixel checker expects an outdated yellow caption and fails
identically at original37d36f6; no silent tolerance/check relaxation. Native
restart semantics, gap-time checks and new checkpoint UI checks pass. This
inherited checker limitation is documented in canonical delivery notes.

All evidence is synthetic PC/native only. No target compile or physical-board
validation is claimed. Original main checkout and its uncommitted work preserved.
