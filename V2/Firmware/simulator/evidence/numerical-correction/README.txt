Corrected nominal three-metre stop travel — synthetic PC evidence
================================================================

rejected-minimized.log: deterministic public benchmark failures before correction.
rejected-full-stress.json: original full independent stress rejection; original
raw values also remain in ../ticket13/rejected-stress-full-schedule.jsonl.
accepted-minimized.log: five schedules now match every stop slot and TTS exactly.
accepted-full-stress.json/accepted-observations.jsonl: unchanged full comparison,
including the original569 observations and9extreme observations; no skipped
assertions, tolerance edits or frozen expected-value changes.

Independent reference author supplied independent-audit.txt,
independent-highprecision.json and highprecision_audit.py.65-digit tissue/ceiling
math agrees exactly with the frozen binary64 oracle on all stop slots and TTS.
To rerun its source audit, place ../../tests/bench/gf-reference on PYTHONPATH;
the script writes highprecision-audit.json beside itself. The retained result
here is the independently supplied independent-highprecision.json.

Narrow correction: only explicitly marked mandatory3m stop-grid travel uses
ceil(60.0*3.0/ascent_rate). At9m/min this is exactly20s. No epsilon snapping,
arbitrary input rounding, tissue arithmetic rewrite, cap change or GF range
restriction. Upstream source/fixed comparison variants and legacy evidence stay
unchanged. See ../../../ostc4_baseline/PLANNER-TIMING-CORRECTION.txt.

Policyv3 includes exact-three-metre-stop-travel-v1 in checkpoint fingerprint;
schema2/296-byte representation unchanged. prior-policy-rejection.log confirms a
valid old-policyCRC checkpoint is not silently accepted by a new process.
headless-before-ticket15.log is the20-test run before merging disk controls;
combined final evidence is recorded separately after integration.

All evidence is synthetic PC mathematics/process validation; no target or
physical-watch validation is claimed.
