Ticket 09: provenance and distribution audit, 2026-10-28
======================================================
Experimental source identity verified; final adoption and release NOT approved.
Canonical dossier:
/home/Bilal/Documents/apps/Markdown files/spec-decompression-engine/implementation/ostc4-provenance-license-09.md
Manufacturer investigation:
/home/Bilal/Documents/apps/Markdown files/spec-decompression-engine/implementation/ostc4-manufacturer-provenance-09.md

Evidence: evidence/ticket09/ (SHA256SUMS.json records all files).
11/11 selected upstream files match the pinned audit archive/manifest.
Official manufacturer stable correspondence and Hg mapping remain unverified.
Current configured firmware links Nordic-5-Clause NCS boot-banner code; resolve
GPL compatibility before release. Further open items include buehlmann.c grant
scope, dated modification notices, runtime source/member license mapping,
complete-watch dependency review and applicable Installation Information.
See the dossier for the actionable checklist and exact scope limitations.

Fresh target compilation/link succeeds. Host regression suite: 14/14 passed.
No physical execution or new independent numerical acceptance is claimed.
This evidence is not a complete Corresponding Source distribution package.

Reproduce collection (uses the pinned original research bundle and installed SDK):
  python3 V2/Firmware/tools/build_target.py --build-dir /tmp/ticket09/target
  # Capture build output at /tmp/ticket09/target-build.log.
  python3 V2/Firmware/ostc4_baseline/evidence/ticket09/collect.py
Collector writes /tmp/ticket09/evidence, never changes upstream or SDK source.
Research responses and regression logs are separately preserved in this bundle.
Raw files are archived byte-for-byte in raw-evidence.tar.gz; extract it before
following raw-file references. raw-evidence-index.json records each raw hash.
After adding research/regression logs to collector output, package with package.py.
