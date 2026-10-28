# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare public pressure/time replay observations to an independent reference.

Default exits nonzero on numerical rejection. --record permits publishing a
failed evidence bundle; its acceptance flag and discrepancies remain unchanged.
"""
import argparse
import csv
import hashlib
import json
import math
import platform
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent
POLICY = json.loads((HERE/'comparison-policy.json').read_text())
TOLERANCES = POLICY['comparisons']


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare_observation(actual, expected):
    discrepancies = []
    def check(field, target, tolerance=0):
        value = actual.get(field)
        if isinstance(target,(list,tuple)):
            bad = not isinstance(value,list) or len(value)!=len(target) or any(
                not isinstance(a,(int,float)) or not math.isfinite(a) or abs(a-e)>tolerance
                for a,e in zip(value,target))
        elif isinstance(target,(int,float)):
            bad = not isinstance(value,(int,float)) or not math.isfinite(value) or abs(value-target)>tolerance
        else:
            bad = value != target
        if bad:
            discrepancies.append(dict(field=field, actual=value, expected=target, tolerance=tolerance))
    for field in ('timestamp_seconds','dive_seconds','availability','ndl_seconds','stop_seconds'):
        check(field,expected[field])
    check('tts_seconds',expected['tts_seconds'],TOLERANCES['tts_seconds'])
    check('absolute_pressure_bar',expected['absolute_pressure_bar'],1e-6)
    check('nitrogen_bar',expected['nitrogen_bar'],TOLERANCES['tissues_abs_bar'])
    check('helium_bar',expected['helium_bar'],0)
    for field in ('gf_anchor_ambient_bar','gf_low_depth_bar'):
        check(field,expected[field],TOLERANCES['anchor_abs_bar'])
    # A no-deco ceiling is explicitly suppressed by the upstream contract.
    target = expected['analytical_ceiling_meters'] if expected['tts_seconds'] else 0
    check('ceiling_meters',target,TOLERANCES['analytical_ceiling_abs_m'])
    return discrepancies


def compare_trace(actual, expected):
    differences = []
    if len(actual)!=len(expected):
        differences.append(dict(field='observation_count',actual=len(actual),expected=len(expected)))
    for a,e in zip(actual,expected):
        for difference in compare_observation(a,e):
            differences.append(dict(timestamp_seconds=e['timestamp_seconds'],**difference))
    return differences


def evaluate(executable, output=None):
    from schedule_reference import Reference
    provenance = json.loads((HERE/'schedule-provenance.json').read_text())
    for name, digest in provenance['frozen_sha256'].items():
        if sha(HERE/name) != digest:
            raise ValueError('reference/policy changed after freeze: '+name)
    reports = []
    if output:
        output.mkdir(parents=True,exist_ok=True)
    for profile in sorted((HERE/'profiles').glob('*.csv')):
        with profile.open() as stream:
            rows = [(int(row['timestamp_seconds']),float(row['absolute_pressure_bar'])) for row in csv.DictReader(stream)]
        oracle = Reference(rows[0][1],rows[0][0])
        expected = [oracle.observe()] + [oracle.advance(t,p) for t,p in rows[1:]]
        # TTS accounting is checked independently within every projected schedule.
        for row in expected:
            assert row['tts_seconds'] == row['travel_seconds'] + sum(row['stop_seconds'])
        process = subprocess.run([str(executable),str(profile)],text=True,capture_output=True,check=True)
        parsed = [json.loads(line) for line in process.stdout.splitlines()]
        assert parsed[0]['type']=='metadata' and len(parsed)==len(rows)+1
        metadata = parsed[0]
        assert metadata['revision']=='7198d203520f3a3cdccf1ccc0bf8af6475ee4819'
        assert metadata['exposure_policy']==POLICY['production_variant']
        assert metadata['correction_set']=='cached-10s-and-stable-schreiner-v1'
        actual = parsed[1:]
        discrepancies = compare_trace(actual,expected)
        if output:
            (output/(profile.stem+'-production.jsonl')).write_text(process.stdout)
            (output/(profile.stem+'-reference.jsonl')).write_text(''.join(json.dumps(row,allow_nan=False)+'\n' for row in expected))
        reports.append(dict(profile=profile.name,profile_sha256=sha(profile),observations=len(rows),
            production_metadata=metadata,accepted=not discrepancies,discrepancies=discrepancies,
            tts_differences=[dict(timestamp_seconds=e['timestamp_seconds'],actual=a['tts_seconds'],
                expected=e['tts_seconds'],difference_seconds=a['tts_seconds']-e['tts_seconds'])
                for a,e in zip(actual,expected) if a['tts_seconds']!=e['tts_seconds']],
            max_tissue_error_bar=max(abs(a-e) for arow,erow in zip(actual,expected)
                                     for a,e in zip(arow['nitrogen_bar'],erow['nitrogen_bar'])),
            coverage=dict(deco_observations=sum(row['tts_seconds']>0 for row in expected),
                          surface_observations=sum(p==rows[0][1] for _,p in rows),
                          ndl_limited_observations=sum(row['availability']['ndl']=='limited' for row in expected),
                          ceiling_violations=sum(row['analytical_ceiling_meters'] > (row['absolute_pressure_bar']-rows[0][1])*10 + .105 for row in expected),
                          final_underwater_tts_seconds=[row['tts_seconds'] for row in expected if row['absolute_pressure_bar']>rows[0][1]][-1])))
    assert len(reports)==6, 'fixed profile suite must be complete'
    report = dict(accepted=all(row['accepted'] for row in reports),
        scope=POLICY['scope'],policy_sha256=sha(HERE/'comparison-policy.json'),
        executable_sha256=sha(executable),reference_sha256=sha(HERE/'schedule_reference.py'),
        tissue_reference_sha256=sha(HERE/'tissue_reference.py'),profiles=reports,
        no_deco_tts='Raw upstream zero explicitly qualified; reference normalized_tts_seconds adds travel-only estimate. Optional safety stops excluded.',
        target_build='not run',physical_execution='not run',reference_provenance=provenance,
        host_runtime=dict(platform=platform.platform(),libc=platform.libc_ver()))
    if output:
        (output/'schedule-comparison.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    parser.add_argument('--output',type=Path)
    parser.add_argument('--record',action='store_true',help='publish even a failed numerical comparison (does not accept it)')
    args = parser.parse_args()
    report = evaluate(args.executable.resolve(),args.output)
    print(json.dumps(dict(accepted=report['accepted'],profiles=[dict(profile=r['profile'],accepted=r['accepted'],discrepancies=len(r['discrepancies'])) for r in report['profiles']]),indent=2))
    raise SystemExit(0 if report['accepted'] or args.record else 1)

if __name__=='__main__':
    main()
