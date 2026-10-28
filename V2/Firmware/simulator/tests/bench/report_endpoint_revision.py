#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare raw public-bench runs from before/after the endpoint precision change."""
import argparse
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('before', type=Path)
parser.add_argument('after', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
before = [json.loads(line) for line in args.before.read_text().splitlines()]
after = [json.loads(line) for line in args.after.read_text().splitlines()]
assert before and len(before) == len(after), 'missing or unmatched observations'
endpoints = ('gf_low_ceiling_meters', 'gf_high_ceiling_meters')
failures, differences = [], []
for old, new in zip(before, after):
    assert old['case'] == new['case'], 'unmatched case order'
    assert old['expected_endpoints_m'] == new['expected_endpoints_m'], 'reference changed'
    old_state = {k: v for k, v in old['actual'].items() if k not in endpoints}
    new_state = {k: v for k, v in new['actual'].items() if k not in endpoints}
    if old_state != new_state:
        failures.append(dict(case=new['case'], changed_fields=[
            key for key in old_state.keys() | new_state.keys()
            if old_state.get(key) != new_state.get(key)]))
    for index, field in enumerate(endpoints):
        previous, current = old['actual'][field], new['actual'][field]
        expected = new['expected_endpoints_m'][index]
        delta, error = current - previous, current - expected
        if not all(math.isfinite(value) for value in (previous, current, expected)) or abs(error) > 0.006:
            failures.append(dict(case=new['case'], field=field, error=error))
        differences.append(dict(case=new['case'], field=field, before=previous,
                                after=current, reference_binary64=expected,
                                delta_m=delta, reference_error_m=error))
report = dict(arithmetic_revision='endpoint-binary32-v1', observations=len(after),
              endpoint_comparisons=len(differences), tolerance_m=0.006,
              max_revision_delta_m=max(abs(d['delta_m']) for d in differences),
              max_reference_error_m=max(abs(d['reference_error_m']) for d in differences),
              unchanged_non_endpoint_state=not any('changed_fields' in f for f in failures),
              failures=failures, raw_differences=differences)
args.output.write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({k: v for k, v in report.items() if k != 'raw_differences'}, indent=2))
raise SystemExit(bool(failures))
