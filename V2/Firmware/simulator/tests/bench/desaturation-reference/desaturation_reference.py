# SPDX-License-Identifier: GPL-3.0-or-later
"""Independently authored ticket17 oracle. No production routine or new code read.
Exposure math imports only frozen independent tissue_reference.py (sha in provenance).
Desaturation algebra derives from pinned OSTC4 convention; binary64 is primary.
Binary32 expression diagnostic models the pinned expression, without executing it.
"""
import datetime, hashlib, json, math, pathlib, struct
from tissue_reference import N2_HALF_MIN, HE_HALF_MIN, replay_reference
ROOT=pathlib.Path(__file__).parent

def f32(x): return struct.unpack('f',struct.pack('f',x))[0]

def estimate(tissues, surface=1.01325, helium=None):
    """Algebraic criterion solution, independent from C expression sequence."""
    baseline=.7902*(surface-.0627)
    helium=helium or [0.]*16
    n=[0. if p<=1.05*baseline else h*math.log((p-baseline)/(.05*baseline))/.6931*1.42 for p,h in zip(tissues,N2_HALF_MIN)]
    he=[0. if p<=.1 else h*math.log(p/.1)/.6931*1.42 for p,h in zip(helium,HE_HALF_MIN)]
    raw=max(n+he)
    return {'minutes':int(raw),'unquantized_minutes':raw,'limiting_compartment_1based':(n+he).index(raw)%16+1,'nitrogen_minutes':n,'helium_minutes':he,'distance_to_integer_minutes':min(raw%1,1-raw%1) if raw else None}

def expression32(tissues,surface=1.01325):
    """Rounding diagnostic only. math.log rounded to f32, not production logf call."""
    b=f32(f32(.7902)*f32(f32(surface)-f32(.0627)))
    q=f32(f32(100.)/f32(142.))
    values=[]
    for p,h in zip(tissues,N2_HALF_MIN):
        p=f32(p); numerator=f32(f32(f32(1.05)*b)-p); denominator=f32(b-p)
        r=f32(numerator/denominator) if denominator<0 else 0.
        if r>0:
            arg=f32(1-r)
            if arg<=0: return {'available':False,'reason':'nonpositive logarithm argument','log_argument':arg}
            raw=f32(f32(f32(h)*f32(f32(math.log(arg))/f32(-.6931)))/q)
        else: raw=0.
        values.append(raw)
    return {'available':True,'minutes':int(max(values)),'unquantized_minutes':max(values)}

def dense(knots,cadence=60):
    rows=[knots[0]]
    for (t0,p0),(t1,p1) in zip(knots,knots[1:]):
        for t in range(t0+cadence,t1,cadence): rows.append((t,p0+(p1-p0)*(t-t0)/(t1-t0)))
        rows.append((t1,p1))
    return rows

def generate():
    surface=1.01325
    cases={
        'surface-zero': ([(0,surface),(60,surface),(3600,surface)], [0,60,3600]),
        'loaded-surface': (dense([(0,surface),(120,surface+3),(2220,surface+3),(2400,surface),(6000,surface)]),[2400,2460,2520,2700,3000,3600,4200,6000]),
        'repetitive': (dense([(0,surface),(120,surface+2),(1320,surface+2),(1500,surface),(3300,surface),(3420,surface+2),(4620,surface+2),(4800,surface),(6600,surface)]),[1500,1560,1800,2400,3300,4800,4860,5100,5400,6600]),
        'long-loaded-surface': (dense([(0,surface),(120,surface+3),(14520,surface+3),(14700,surface),(18300,surface)]),[14700,14760,14820,15300,16500,18300])}
    result={}
    for name,(rows,selected) in cases.items():
        csv='timestamp_seconds,absolute_pressure_bar\n'+''.join(f'{t},{p:.12g}\n' for t,p in rows)
        (ROOT/(name+'.csv')).write_text(csv)
        (ROOT/(name+'.commands.txt')).write_text('fresh 30 70\n'+''.join(f'advance {t} {p:.12g}\n'+('finish\n' if t in ([2400] if name=='loaded-surface' else [1500,4800] if name=='repetitive' else [14700] if name=='long-loaded-surface' else []) else '')+('arm\n' if name=='repetitive' and t==3300 else '') for t,p in rows[1:]))
        allpoints=[]
        for (t,p),tissues in zip(rows,replay_reference(rows)):
            e=estimate(tissues,surface)
            allpoints.append({'timestamp_seconds':t,'absolute_pressure_bar':p,'nitrogen_bar':tissues,'helium_bar':[0.]*16,'expected':e,'binary32_expression_diagnostic':expression32(tissues,surface)})
        result[name]={'surface_bar':surface,'csv_sha256':hashlib.sha256(csv.encode()).hexdigest(),'selected_checkpoints':[p for p in allpoints if p['timestamp_seconds'] in selected],'all_observations':allpoints}
    # Independent minute-boundary inputs: isolated compartment16, others at baseline.
    b=.7902*(surface-.0627)
    boundaries=[]
    for minute in (0,1,59,60,61,754,1500):
        for offset in ((.5,) if minute==0 else (-.01,.01,.5)):
            target=minute+offset
            tissues=[b]*16
            tissues[-1]=b+.05*b*math.exp(target*.6931/(635*1.42))
            boundaries.append({'target_unquantized_minutes':target,'nitrogen_bar':tissues,'expected':estimate(tissues,surface),'binary32_expression_diagnostic':expression32(tissues,surface),'purpose':'algebraic quantization analysis; do not inject private state into bench'})
    # Rounded 1-r can become zero for unrealistic but finite tissue values. Never
    # turn log(0), infinity or float->int overflow into a valid numeric estimate.
    hazards=[]
    for pressure in (1e5,1e6,1e7,1e8):
        tissues=[b]*16; tissues[-1]=pressure
        hazards.append({'nitrogen_bar_last':pressure,'binary64':estimate(tissues,surface),'binary32_expression_diagnostic':expression32(tissues,surface)})
    frozen=datetime.datetime.now(datetime.timezone.utc).isoformat()
    output={'frozen_at_utc':frozen,'acceptance_policy':'Exact integer minute equality at selected away-boundary profile checkpoints. No new numerical tolerance. Boundary fixtures are explicit diagnostics; production output was not read before freeze.','profiles':result,'minute_boundary_analysis':boundaries,'finite_domain_hazards':hazards}
    (ROOT/'expected.json').write_text(json.dumps(output,indent=2)+'\n')
    provenance={'frozen_at_utc':frozen,'author':'independent desaturation_reference agent; no new production desaturation implementation read','pinned_revision':'7198d203520f3a3cdccf1ccc0bf8af6475ee4819','pinned_source':'V2/Firmware/ostc4_baseline/upstream/Common/Src/decom.c','pinned_source_sha256':'f7c95aa71598f05905f6a2737eb8ca3a4346b4563d5404f66f3c167e9a71c6cd','source_lines':{'nitrogen_half_times':'154-170','helium_half_times':'172-188','selected_desaturation_convention':'1121-1229'},'independent_tissue_reference_sha256':hashlib.sha256((ROOT/'tissue_reference.py').read_bytes()).hexdigest(),'independent_tissue_reference_provenance':'reference-provenance.json; authored and frozen 2026-10-27 before reading production arrays; coefficient photograph and Baker Fortran sources','primary_math':'Binary64 analytical tissue integration, then baseline B=.7902*(surface-.0627). N2 time=0 if P<=1.05*B, otherwise h*ln((P-B)/(.05*B))/.6931*1.42; He time=0 if P<=.1 else hHe*ln(P/.1)/.6931*1.42; truncate positive minutes and max across16. Product profiles He=0.','rounding_diagnostic':'Separate binary32 rounded expression model, no production function call. Not a replacement independent tissue engine and not an allowance.','oracle_sha256':hashlib.sha256(pathlib.Path(__file__).read_bytes()).hexdigest(),'scope':'Synthetic PC numerical reference only, no production comparison yet, no target or physical execution, no operational clearance claim.'}
    (ROOT/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
    print(json.dumps({name:[(p['timestamp_seconds'],p['expected']['minutes'],round(p['expected']['unquantized_minutes'],5),p['binary32_expression_diagnostic'].get('minutes')) for p in item['selected_checkpoints']] for name,item in result.items()},indent=2))

if __name__=='__main__': generate()
