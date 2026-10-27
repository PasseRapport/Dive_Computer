# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent public-CLI experiments and raw original/corrected comparison.
Usage: python3 verify_tissue_replay.py BASELINE [CORRECTED] --output report.json
"""
import argparse, hashlib, json, math, pathlib, subprocess, tempfile
from tissue_reference import *


def sha(path): return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def csv_text(rows):
    return 'timestamp_seconds,absolute_pressure_bar\n'+''.join(f'{t},{p:.12g}\n' for t,p in rows)


def profiles():
    result={}
    durations=DURATIONS+(2,4,9,11,19,21,59,61,99,101,299,301,799,801,810,3599)
    for dt in durations:
        result[f'uptake-{dt}']=[(0,1.),(120,4.),(120+dt,4.)]
        result[f'elimination-{dt}']=[(0,1.),(120,4.),(3720,4.),(3840,1.),(3840+dt,1.)]
    for dt in DURATIONS:
        for surface in (.8,1.,1.2):
            result[f'ramp-both-{dt}-{surface}']=[(0,surface),(dt,surface+3.),(2*dt,surface)]
    result['ramp-short-extreme']=[(0,1.01325),(1,7.01325),(2,1.01325)]
    # Same linear pressure history, three sampling cadences. No reset at surface.
    knots=[(0,1.),(120,4.),(1320,4.),(1500,1.),(3300,1.),(3420,3.5),(4320,3.5),(4500,1.),(6300,1.)]
    for cadence in (10,20,60):
        rows=[knots[0]]
        for (t0,p0),(t1,p1) in zip(knots,knots[1:]):
            for t in range(t0+cadence,t1,cadence):rows.append((t,p0+(p1-p0)*(t-t0)/(t1-t0)))
            rows.append((t1,p1))
        result[f'repetitive-cadence-{cadence}']=rows
    result['surface-reset-convention']=[(0,1.),(1,1.),(3601,1.),(7201,1.)]
    result['shallow-half-time']=[(0,1.),(600,2.),(900,2.),(1200,1.)]
    return result


def run(executable,rows,directory,name):
    path=directory/(name+'.csv');path.write_text(csv_text(rows))
    output=subprocess.check_output([str(executable),str(path)],text=True)
    parsed=[json.loads(x) for x in output.splitlines()]
    observations=[x for x in parsed if x['type']=='observation']
    assert len(observations)==len(rows)
    return parsed[0],observations,output


def analytical_checks(rows,observations):
    expected=replay_reference(rows);points=[];bad=[];local_bad=[]
    tol=HISTORY_ABS_BAR if len(rows)>10 else SINGLE_ABS_BAR
    for n,(row,actual,want) in enumerate(zip(rows,observations,expected)):
        errors=[a-e for a,e in zip(actual['nitrogen_bar'],want)]
        local=[]
        if n:
            (t0,p0),(t1,p1)=rows[n-1:n+1]
            local=[a-segment(v,p0,p1,t1-t0,h) for a,v,h in zip(actual['nitrogen_bar'],observations[n-1]['nitrogen_bar'],N2_HALF_MIN)]
        point={'timestamp_seconds':row[0],'nitrogen_error_bar':errors,'local_segment_error_bar':local,'helium_bar':actual['helium_bar']}
        points.append(point)
        for i,e in enumerate(errors,1):
            if abs(e)>tol:bad.append({'timestamp_seconds':row[0],'compartment':i,'error_bar':e})
        for i,e in enumerate(local,1):
            if abs(e)>SINGLE_ABS_BAR:local_bad.append({'timestamp_seconds':row[0],'compartment':i,'error_bar':e})
        assert all(math.isfinite(v) for v in actual['nitrogen_bar'])
        assert all(v==0 for v in actual['helium_bar'])
    return {'tolerance_bar':tol,'pass':not bad and not local_bad,'max_absolute_error_bar':max(abs(e) for p in points for e in p['nitrogen_error_bar']),'violations':bad,'local_segment_violations':local_bad,'points':points}


def cadence_check(runs):
    by_time={c:{o['timestamp_seconds']:o for o in values} for c,values in runs.items()}
    common=sorted(set.intersection(*(set(v) for v in by_time.values())))
    errors=[]
    for t in common:
        for left,right in ((10,20),(10,60),(20,60)):
            errors.append({'timestamp_seconds':t,'cadences_seconds':[left,right],'nitrogen_delta_bar':[b-a for a,b in zip(by_time[left][t]['nitrogen_bar'],by_time[right][t]['nitrogen_bar'])]})
    worst=max(abs(e) for point in errors for e in point['nitrogen_delta_bar'])
    return {'tolerance_bar':HISTORY_ABS_BAR,'common_timestamp_count':len(common),'max_absolute_error_bar':worst,'pass':worst<=HISTORY_ABS_BAR,'points':errors}


def differences(a,b):
    result=[]
    fields=('nitrogen_bar','helium_bar','ndl_seconds','tts_seconds','ceiling_meters','stop_seconds','gf_anchor_ambient_bar','gf_low_depth_bar','availability')
    for x,y in zip(a,b):
        changed={}
        for field in fields:
            if x[field]!=y[field]:
                if isinstance(x[field],list):
                    changed[field]=[{'index':i,'original':v,'corrected':w,'delta':w-v} for i,(v,w) in enumerate(zip(x[field],y[field])) if v!=w]
                elif isinstance(x[field],(float,int)):
                    changed[field]={'original':x[field],'corrected':y[field],'delta':y[field]-x[field]}
                else:changed[field]={'original':x[field],'corrected':y[field]}
        if changed:result.append({'timestamp_seconds':x['timestamp_seconds'],'changes':changed})
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('baseline',type=pathlib.Path);parser.add_argument('corrected',type=pathlib.Path,nargs='?');parser.add_argument('--output',type=pathlib.Path,required=True);parser.add_argument('--fixtures',type=pathlib.Path);parser.add_argument('--original-source',type=pathlib.Path);parser.add_argument('--corrected-source',type=pathlib.Path);args=parser.parse_args()
    here=pathlib.Path(__file__).parent
    report={'reference_sha256':sha(here/'tissue_reference.py'),'provenance':json.loads((here/'reference-provenance.json').read_text()),'executables':{'original':{'path':str(args.baseline),'sha256':sha(args.baseline)}},'profiles':{},'scope':'Synthetic PC tissue verification only. Schedule deltas are observations, not independent schedule acceptance; no target or physical validation.'}
    if args.original_source or args.corrected_source:
        from audit_tissue_tables import audit
        report['source_audits']={}
        if args.original_source:report['source_audits']['original']=audit(args.original_source)
        if args.corrected_source:report['source_audits']['corrected']=audit(args.corrected_source)
    if args.corrected:report['executables']['corrected']={'path':str(args.corrected),'sha256':sha(args.corrected)}
    cadence_runs={'original':{},'corrected':{}}
    cases=profiles()
    if args.fixtures:
        for name in ('shallow','decompression'):
            lines=(args.fixtures/(name+'.csv')).read_text().splitlines()[1:]
            cases['fixture-'+name]=[(int(a),float(b)) for a,b in (line.split(',') for line in lines)]
    with tempfile.TemporaryDirectory(prefix='ostc4-analytical-') as temp:
        directory=pathlib.Path(temp)
        for name,rows in cases.items():
            item={'input_csv':csv_text(rows),'input_sha256':hashlib.sha256(csv_text(rows).encode()).hexdigest()}
            md,original,raw=run(args.baseline,rows,directory,name)
            assert md['correction_set']=='none' and md['policy']=='ostc4-beta-7198d20-unchanged-air-gf30-70-v1'
            if name.startswith('repetitive-cadence-'):cadence_runs['original'][int(name.rsplit('-',1)[1])]=original
            item['original']={'metadata':md,'output_sha256':hashlib.sha256(raw.encode()).hexdigest(),'analytical':analytical_checks(rows,original)}
            if args.corrected:
                md,corrected,raw=run(args.corrected,rows,directory,name)
                assert md['correction_set']=='cached-10s-and-stable-schreiner-v1' and md['policy']=='ostc4-beta-7198d20-tissue-corrected-air-gf30-70-v1'
                if name.startswith('repetitive-cadence-'):cadence_runs['corrected'][int(name.rsplit('-',1)[1])]=corrected
                item['corrected']={'metadata':md,'output_sha256':hashlib.sha256(raw.encode()).hexdigest(),'analytical':analytical_checks(rows,corrected)}
                item['original_corrected_differences']=differences(original,corrected)
            report['profiles'][name]=item
    report['cadence_comparisons']={v:cadence_check(runs) for v,runs in cadence_runs.items() if runs}
    for variant in ('original','corrected'):
        if variant not in report['executables']:continue
        checks=[p[variant]['analytical'] for p in report['profiles'].values()]
        report[variant+'_summary']={'profiles':len(checks),'observation_count':sum(len(x['points']) for x in checks),'failed_profiles':{name:p[variant]['analytical']['max_absolute_error_bar'] for name,p in report['profiles'].items() if not p[variant]['analytical']['pass']},'max_absolute_error_bar':max(x['max_absolute_error_bar'] for x in checks)}
    args.output.write_text(json.dumps(report,separators=(',',':'))+'\n')
    print(json.dumps({k:v for k,v in report.items() if k.endswith('_summary')},indent=2))

if __name__=='__main__':main()
