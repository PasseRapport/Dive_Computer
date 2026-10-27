# SPDX-License-Identifier: GPL-3.0-or-later
"""Post-freeze audit of literal tables, never used to construct the oracle."""
import argparse, hashlib, json, math, pathlib, re
from tissue_reference import *


def audit(path):
    source = pathlib.Path(path).read_text()
    def array(name):
        match=re.search(r'const\s+float\s+'+re.escape(name)+r'\s*\[[^]]*\]\s*=\s*\{([^}]+)\}', source)
        if match is None: raise ValueError('missing '+name)
        return [float(v.strip().rstrip('fF')) for v in match[1].split(',') if v.strip()]
    checks=[]
    expected_arrays={'buehlmann_N2_a':N2_A_C,'buehlmann_N2_b':N2_B,'buehlmann_N2_t_halflife':N2_HALF_MIN,'buehlmann_He_t_halflife':HE_HALF_MIN,'nitrogen_time_constant':tuple(math.log(2)/h for h in N2_HALF_MIN),'helium_time_constant':tuple(math.log(2)/h for h in HE_HALF_MIN)}
    for name,expected in expected_arrays.items():
        actual=array(name)
        assert len(actual)==len(expected)==16
        for i,(a,e) in enumerate(zip(actual,expected),1):
            error=abs(a-e)
            tol=K_ABS_TOL*60 if name.endswith('time_constant') else COEFFICIENT_ABS_TOL
            checks.append({'array':name,'compartment':i,'literal':a,'expected':e,'absolute_error':error,'tolerance':tol,'pass':error<=tol,'primary_source':'baker-gfdeco' if 'time_constant' in name or 'He_' in name else 'buhlmann-table26'})
    suffixes=('one_second','003_second','008_second','10_seconds','18_seconds','20_seconds','one_minute','100_second','five_minutes','800_second','one_hour')
    for gas,halves in (('N2',N2_HALF_MIN),('He',HE_HALF_MIN)):
        for duration,suffix in zip(DURATIONS,suffixes):
            name=f'float_buehlmann_{gas}_factor_expositon_{suffix}'
            actual=array(name)
            assert len(actual)==16
            for i,(a,h) in enumerate(zip(actual,halves),1):
                e=exposure_factor(duration,h);error=abs(a-e)
                checks.append({'array':name,'gas':gas,'duration_seconds':duration,'compartment':i,'literal':a,'expected':e,'replacement_literal':f'{e:.14E}f','absolute_error':error,'relative_error_percent':100*(a-e)/e,'tolerance':FACTOR_ABS_TOL,'pass':error<=FACTOR_ABS_TOL,'primary_source':'baker-gfdeco half-times; independently derived 1-exp(-ln(2)*seconds/(60*half_min))'})
    return {'source_sha256':hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest(),'classification':'ZH-L16C with optional compartment 1b (5 min N2 / 1.88 min He), not four-minute model and not ZH-L16B','coefficient_checks':96,'factor_checks':352,'checks':checks,'failures':[x for x in checks if not x['pass']]}

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('source');parser.add_argument('output');args=parser.parse_args()
    result=audit(args.source);pathlib.Path(args.output).write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'checked':len(result['checks']),'failures':result['failures']},indent=2))
