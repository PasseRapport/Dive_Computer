# SPDX-License-Identifier: GPL-3.0-or-later
"""Freeze expected GF vectors without importing or executing production code."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json
from configurable_reference import Reference, GFPair, SURFACE, ceiling_pressure

ROOT = Path(__file__).parent


def event(reference, action, **inputs):
    if action == 'advance':
        result = reference.advance(inputs['timestamp_seconds'], inputs['absolute_pressure_bar'])
    elif action == 'set_gf_pair':
        result = reference.set_gf_pair(*inputs['gf_pair'])
    else:
        result = reference.observe()
    return dict(action=action, **inputs, expected=result)


def profile(identity, pair, segments, note):
    r = Reference(gf_low=pair[0], gf_high=pair[1])
    events = [event(r, 'initialize')]
    for seconds, depth in segments:
        events.append(event(r, 'advance', timestamp_seconds=seconds, absolute_pressure_bar=round(SURFACE+depth/10, 9)))
    return dict(identity=identity, initial_gf_pair=pair, note=note, events=events)


cases = []
pairs = [(20,60), (30,70), (40,85), (70,70), (100,100)]
for pair in pairs:
    label = '-'.join(map(str,pair))
    cases.append(profile('shallow-'+label, pair, [(30,2),(59,2),(60,2),(61,2),(660,2),(720,0)],
                         'Fresh initialization; 59/60/61 schedule availability boundary; endpoint math remains from current tissues.'))
    cases.append(profile('decompression-'+label, pair,
                         [(120,30),(420,30),(720,30),(1020,30),(1320,30),(1620,30),(1920,30),(2220,30),(2380,6),(2680,6),(3000,3)],
                         'Matched explicit exposure/observation cadence; fixed fixture holds are inputs, never expected mandatory stops.'))

r = Reference()
events = [event(r, 'initialize')]
for timestamp, depth in [(120,30),(2220,30),(2380,6),(2980,6),(3000,3),(4500,3),(4520,0),(4640,0)]:
    events.append(event(r,'advance',timestamp_seconds=timestamp, absolute_pressure_bar=round(SURFACE+depth/10,9)))
before = r.observe()
events.append(event(r,'set_gf_pair',gf_pair=[20,60]))
after = r.observe()
assert before['nitrogen_bar'] == after['nitrogen_bar']
assert before['timestamp_seconds'] == after['timestamp_seconds']
assert before['gf_anchor_ambient_bar'] == after['gf_anchor_ambient_bar']
assert before['gf_high_ceiling_meters'] == 0
assert after['gf_high_ceiling_meters'] > 0
assert after['tts_seconds'] > 0
for timestamp,depth in [(4760,18),(5360,18),(5960,18),(6560,18)]:
    events.append(event(r,'advance',timestamp_seconds=timestamp,absolute_pressure_bar=round(SURFACE+depth/10,9)))
cases.append(dict(identity='repetitive-retained-anchor-30-70-to-20-60',initial_gf_pair=[30,70],
                  note='Complete/unlock the recorded dive at surface before the same-time policy change. Recorded lifecycle is tested separately. More-conservative GF High exposes residual obligations after the previous High ceiling clears. Surface projected schedules retain legacy unavailable validity even when internal residual obligations are nonzero.',events=events))

fresh = Reference(gf_low=20,gf_high=60)
fresh_events = [event(fresh,'initialize')]
for timestamp,depth in [(120,18),(720,18),(1320,18),(1920,18)]:
    fresh_events.append(event(fresh,'advance',timestamp_seconds=timestamp,absolute_pressure_bar=round(SURFACE+depth/10,9)))
cases.append(dict(identity='fresh-control-second-dive-20-60',initial_gf_pair=[20,60],
                  note='Control only: same second-dive exposure duration from fresh surface saturation; compare to residual-loaded second dive, never use this fresh state to replace it.',events=fresh_events))

endpoint_vectors=[]
for label, tissues in [('fresh-surface',(.7902*(SURFACE-.0493),)*16),
                       ('uniform-high-loading',(4.5,)*16),
                       ('loaded-first-dive',cases[3]['events'][8]['expected']['nitrogen_bar'])]:
    for pair in [(1,1),(1,100),(20,60),(30,70),(70,70),(99,100),(100,100)]:
        low=max(0.,10*(ceiling_pressure(tissues,pair[0]/100)-SURFACE))
        high=max(0.,10*(ceiling_pressure(tissues,pair[1]/100)-SURFACE))
        endpoint_vectors.append(dict(identity=label+'-'+'-'.join(map(str,pair)),surface_bar=SURFACE,
                                     nitrogen_bar=list(tissues),gf_pair=pair,gf_low_ceiling_meters=low,
                                     gf_high_ceiling_meters=high,display_low=f'{low:.1f}',display_high=f'{high:.1f}'))

output=dict(identity='independent-configurable-gf-vectors-v1',generated_utc=datetime.now(timezone.utc).isoformat(),
            policy_file='comparison-policy-gf.json',cases=cases,endpoint_only_vectors=endpoint_vectors,
            endpoint_invalid_contract=[dict(input_state=s,expected_availability='unavailable',numeric_value_is_not_valid=True)
                                       for s in ['uninitialized','exposure-continuity-unknown','latched-invalid-input']],
            display_unavailable='--.- / --.- m')
(ROOT/'gf-vectors.json').write_text(json.dumps(output,indent=2)+'\n')
provenance=dict(frozen_utc=datetime.now(timezone.utc).isoformat(),production_inspected=False,
                independence='Expected values originate only from independently authored binary64 Python reference and fixed-GF inequality. No production binaries or outputs are consulted.',
                sha256={name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
                        ('comparison-policy-gf.json','tissue_reference.py','schedule_reference.py','configurable_reference.py','generate_gf_vectors.py','gf-vectors.json')})
(ROOT/'gf-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
print(json.dumps(dict(cases=len(cases),events=sum(len(c['events']) for c in cases),endpoint_vectors=len(endpoint_vectors))))
