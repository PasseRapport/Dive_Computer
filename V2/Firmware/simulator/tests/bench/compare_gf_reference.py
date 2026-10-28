#!/usr/bin/env python3
"""Compare public bench observations against independently frozen expectations."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).parent / 'gf-reference'
POLICY = json.loads((ROOT / 'comparison-policy-gf.json').read_text())['comparisons']
FIXTURES = json.loads((ROOT / 'gf-vectors.json').read_text())
PROVENANCE = json.loads((ROOT / 'gf-provenance.json').read_text())
for name, digest in PROVENANCE['sha256'].items():
    assert hashlib.sha256((ROOT/name).read_bytes()).hexdigest() == digest, name
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT))
from configurable_reference import Reference

failures = []
extra_events = 0
differences = []
raw_records = []

def assess(label, actual, expected):
    raw_records.append(dict(case=label, actual=actual, expected_tts_seconds=expected["tts_seconds"]))
    exact = ('gf_pair', 'timestamp_seconds', 'dive_seconds', 'ndl_seconds', 'stop_seconds', 'availability')
    for field in exact:
        if actual[field] != expected[field]:
            failures.append(f'{label}: {field}: actual={actual[field]} expected={expected[field]}')
    bounds = {'tts_seconds': POLICY['tts_seconds'], 'ceiling_meters': POLICY['operational_ceiling_abs_m'],
              'gf_anchor_ambient_bar': POLICY['anchor_abs_bar'], 'gf_low_depth_bar': POLICY['anchor_abs_bar'],
              'absolute_pressure_bar': 0.000001}
    for field, bound in bounds.items():
        difference = actual[field] - expected[field]
        if difference and field == 'tts_seconds':
            differences.append(dict(case=label, field=field, actual=actual[field], expected=expected[field], difference=difference))
        if abs(difference) > bound:
            failures.append(f'{label}: {field}: actual={actual[field]} expected={expected[field]} bound={bound}')
    for i, (actual_tissue, expected_tissue) in enumerate(zip(actual['nitrogen_bar'], expected['nitrogen_bar'])):
        if abs(actual_tissue - expected_tissue) > POLICY['tissues_abs_bar']:
            failures.append(f'{label}: tissue {i}: actual={actual_tissue} expected={expected_tissue}')

for case in FIXTURES['cases']:
    commands = []
    for event in case['events']:
        if event['action'] == 'initialize':
            commands.append('fresh %d %d' % tuple(case['initial_gf_pair']))
        elif event['action'] == 'advance':
            commands.append('advance %d %.9f' % (event['timestamp_seconds'], event['absolute_pressure_bar']))
        elif event['action'] == 'set_gf_pair':
            commands.extend(['finish', 'pair %d %d' % tuple(event['gf_pair']), 'arm'])
    process = subprocess.run([sys.argv[1]], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
    observations = [json.loads(line) for line in process.stdout.splitlines()]
    assert len(observations) == len(case['events'])
    for n, (actual, event) in enumerate(zip(observations, case['events'])):
        assess(f"{case['identity']}[{n}]", actual, event['expected'])

# A separately authored frozen oracle also evaluates the real one-second
# cadence. Its expected values never read production outputs or C arithmetic.
for pair in [(20,60), (30,70), (40,85), (70,70), (100,100)]:
    reference = Reference(gf_low=pair[0], gf_high=pair[1])
    commands = ['fresh %d %d' % pair]
    expected = [reference.observe()]
    for seconds in range(1, 91):
        pressure = round(1.01325 + min(seconds,30)/30*.2, 9)
        commands.append('advance %d %.9f' % (seconds, pressure))
        expected.append(reference.advance(seconds, pressure))
    process = subprocess.run([sys.argv[1]], input='\n'.join(commands)+'\n', text=True, capture_output=True, check=True)
    observations = [json.loads(line) for line in process.stdout.splitlines()]
    assert len(observations) == len(expected)
    for n, (actual, target) in enumerate(zip(observations, expected)):
        assess(f'one-second-shallow-{pair}[{n}]', actual, target)
    extra_events += len(observations)

if len(sys.argv) > 2:
    Path(sys.argv[2]).write_text('\n'.join(json.dumps(record) for record in raw_records)+'\n')

print(json.dumps(dict(identity='configurable-gf-production-comparison-v1',
                     cases=len(FIXTURES['cases']), one_second_cases=5, one_second_events=extra_events, events=sum(len(c['events']) for c in FIXTURES['cases']),
                     tts_differences=differences, failures=failures), indent=2))
if failures:
    sys.exit(1)
