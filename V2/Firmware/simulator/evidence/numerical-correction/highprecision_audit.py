"""Reference convergence audit only: retain route policy, use65-digit tissue math.
Frozen oracle and fixtures are untouched. No production calls are made.
"""
from decimal import Decimal, getcontext
from functools import lru_cache
import json
from pathlib import Path
import configurable_reference as ref

getcontext().prec=65
D=Decimal
LN2=D(2).ln()
ORIGINAL_EXPOSURE=ref.exposure
ORIGINAL_CEILING=ref.ceiling_pressure
A=tuple(D(str(x)) for x in ref.N2_A_C)
B=tuple(D(str(x)) for x in ref.N2_B)
H=tuple(D(str(x)) for x in ref.N2_HALF_MIN)

@lru_cache(None)
def factors(seconds,half):
    x=LN2*D(seconds)/(D(60)*half)
    q=D(1)-(-x).exp()
    return q,D(1)-q/x

def exposure65(tissues,start,end,seconds):
    if seconds==0:return tuple(tissues)
    start,end=D(str(start)),D(str(end))
    inspired=D('.79')*(start-D('.0493'))
    rate_span=D('.79')*(end-start)
    result=[]
    for tissue,half in zip(tissues,H):
        tissue=D(str(tissue));q,ramp=factors(seconds,half)
        result.append(tissue+(inspired-tissue)*q+rate_span*ramp)
    return tuple(result)

def ceiling65(tissues,gf):
    gf=D(str(gf))
    return float(max((D(str(t))-gf*a)/(1+gf*(1/b-1)) for t,a,b in zip(tissues,A,B)))

def run(pair,depth,hold):
    r=ref.Reference(gf_low=pair[0],gf_high=pair[1])
    r.advance(120,ref.SURFACE+depth/10)
    o=r.advance(120+hold,ref.SURFACE+depth/10)
    return {k:o[k] for k in ('tts_seconds','stop_seconds','gf_anchor_ambient_bar','gf_low_depth_bar','travel_seconds','nitrogen_bar')}

records=[]
for pair,depth,hold in [((1,1),60,3600),((1,100),60,3600),((100,100),60,3600),((1,100),60,1800),((1,100),30,3600)]:
    ref.exposure,ref.ceiling_pressure=ORIGINAL_EXPOSURE,ORIGINAL_CEILING
    binary64=run(pair,depth,hold)
    ref.exposure,ref.ceiling_pressure=exposure65,ceiling65
    decimal65=run(pair,depth,hold)
    assert binary64['stop_seconds']==decimal65['stop_seconds']
    assert binary64['tts_seconds']==decimal65['tts_seconds']
    records.append(dict(gf_pair=pair,depth_m=depth,hold_seconds=hold,
                        binary64_tts=binary64['tts_seconds'],decimal65_tts=decimal65['tts_seconds'],
                        stop_slots_identical=True,travel_seconds=binary64['travel_seconds'],
                        stop_seconds=binary64['stop_seconds'],
                        max_tracked_tissue_error_bar=max(abs(float(a)-float(b)) for a,b in zip(binary64['nitrogen_bar'],decimal65['nitrogen_bar']))))
ref.exposure,ref.ceiling_pressure=ORIGINAL_EXPOSURE,ORIGINAL_CEILING
out=Path(__file__).parent/'highprecision-audit.json'
out.write_text(json.dumps(dict(identity='frozen-gf-reference-tissue-precision-convergence-audit',decimal_precision=65,
                            route_timing='unchanged frozen exact rational decimal timing',
                            limitation='Route pressure/control remains binary64; tissue ODE and ceiling coefficient arithmetic use65 digits; no production output used as expected',results=records),indent=2)+'\n')
print(out)
for r in records:print(r['gf_pair'],r['depth_m'],r['hold_seconds'],r['binary64_tts'],r['decimal65_tts'],r['max_tracked_tissue_error_bar'])
