Ticket 17 independently authored numerical reference

Frozen profile expected values: expected.json. Production code and output were
not inspected before these expectations were frozen. The implementation agent
owns importing, executing and comparing these fixtures. No repository was edited.

Read scope: original repository AGENTS.md, canonical tracker/domain/glossary/spec,
accepted endpoint precision ADR, ticket17, pinned upstream desaturation function
and half-times, existing independent tissue_reference.py/provenance and existing
public simulator bench interface/examples. No .work/desaturation-17 code read.

Pinned source revision: 7198d203520f3a3cdccf1ccc0bf8af6475ee4819.
upstream/Common/Src/decom.c SHA256:
f7c95aa71598f05905f6a2737eb8ca3a4346b4563d5404f66f3c167e9a71c6cd
Selected function is decom_calc_desaturation_time(), lines1129-1229, not the
per-tissue helper at789 (which uses a different helium convention).

Independent tissue_reference.py was frozen on2026-10-27 before source-array
inspection; its provenance cites a photographed Buehlmann table and original
Baker Fortran. SHA256:
842186e884ddd3fdba8fd7aff76322f688706de6b5b31ce7ec1da4ed4b77208a

Conventions: N2 baseline B=.7902*(surface_bar-.0627), threshold1.05*B,
ln denominator .6931, multiplier100/142 (divide by it), minutes truncated toward
zero per positive compartment, then maximum across16. Helium threshold .1bar;
all product profile vectors are air-only with helium0. Do not substitute the
exposure model's separate .0493bar water-vapour convention into this formula.
The primary oracle uses independent binary64 tissue integration and an
algebraically derived criterion solution. A separate rounded binary32 expression
model is diagnostic; it calls no production routine and introduces no tolerance.

Each *.csv holds timestamp_seconds and absolute_pressure_bar. The corresponding
*.commands.txt uses the existing gf_bench_replay fresh/advance/finish/arm seam.
All time segments <=3600s and selected completed-dive checkpoints are at surface.
Selected expectations are exact integer equality, with no new tolerance:
loaded-surface: t2400=1343,2460=1342,2520=1340,2700=1337,3000=1330,
3600=1317,4200=1305,6000=1266 minutes.
repetitive: t1500=539,1560=538,1800=533,2400=520,3300=502,4800=1060,
4860=1059,5100=1054,5400=1048,6600=1023 minutes.
long-loaded-surface: t14700=3517,14760=3515,14820=3514,15300=3503,
16500=3475,18300=3433 minutes. 3517min formats58h37min (>24h).
surface-zero: t0,t60,t3600=0minutes.
All selected profile integers also match the independently authored rounded
binary32-expression diagnostic before production comparison. The minimum
nonzero selected distance from a binary64 integer boundary is .06954minute.

Quantization analysis is explicit in expected.json and boundary-analysis.json:
isolated slow-compartment inputs at ideal60-.001minutes produce59 under
binary64 algebra and60 under the float expression; this is a boundary diagnostic,
not a tolerance. Exact inversions such as61 or754 can land just below their
mathematical integer in binary64, so they must not become unconditional integer
acceptance expectations. Half-minute-centered isolated vectors cover exact
format values0,1,59,60,61,754,1500; use existing presentation tests for formatting,
not private tissue injection as a new bench interface.

Known finite-domain hazard: unrealistic N2=1e7bar makes the rounded float
logarithm argument1-r equal0. log(0) is infinite and conversion to int has no
portable valid result. The pinned routine's unchecked cast/max pattern could
therefore falsely collapse to0 on some systems. A nonfinite result must not be
reported as an authoritative numeric zero. The artifact diagnoses this without
calling the production routine. It does not establish that such a state is
reachable through accepted public pressure inputs.

Compare after importing the fixture files together:
python3 compare_public.py PATH_TO_RUNNER --field desaturation_minutes --output report.json
This retains raw public JSONL and integer-minute differences, fails on absent or
unequal selected values, and does not regenerate expected values from production.

Evidence limit: authored numerical fixtures only. No production pass, target
compilation, physical execution or suitability for operational diving is claimed.
Zero is the engine convention, never clearance to dive again.

SHA256SUMS.json freezes every file in this directory except itself and transient
__pycache__. All authored/reference artifacts remain under /tmp/desaturation-reference.
