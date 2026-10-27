#!/usr/bin/env python3
"""Independent virtual-ascent oracle; only verified isolated worker imports upstream."""
import argparse
from decimal import Decimal, localcontext
import hashlib
import importlib
import io
import json
import math
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from runner import verify

RATE = 1025.0 * 9.80665 * 9.0 / 100000.0 / 60.0


def validate(cases):
    if not isinstance(cases, list) or not cases:
        raise ValueError('Expected nonempty transit requests')
    for c in cases:
        if len(c['tissues_bar']) != 16 or c['mode'] not in ('fixed', 'line'):
            raise ValueError('Expected mode and exactly 16 compartments')
        if type(c['allowance']) is not int or not 0 <= c['allowance'] < 2**64:
            raise ValueError('Expected uint64 work allowance')


def gf_at(c, pressure):
    if c['mode'] == 'fixed':
        return c['gf']
    anchor = c.get('anchor')
    if anchor is None:
        return 0.70
    if pressure >= anchor:
        return .30
    if pressure <= c['surface']:
        return .70
    fraction = (pressure-c['surface'])/(anchor-c['surface'])
    return 0.70 - 0.40*fraction


def inspection_offsets(duration):
    """Lazy even for enormous permitted grids that reject at the start."""
    yield 0.0
    yield from (float(i) for i in range(1, math.ceil(duration)))
    if duration:
        yield duration


def decimal_schreiner(captured, start, end, seconds, half_lives):
    """Evaluate the classical Schreiner equation at 100 decimal digits.

    Derived from dP/dt = k*(inspired_start + inspired_rate*t - P).
    No endpoint-weight implementation or production numerical code is used.
    Exact binary64 inputs are lifted into Decimal before any subtraction.
    """
    with localcontext() as ctx:
        ctx.prec = 100
        D = Decimal.from_float
        minutes = D(seconds) / Decimal(60)
        inspired = D(.79) * (D(start) - D(.0627))
        slope = D(.79) * (D(end) - D(start)) / minutes
        values = []
        for initial, half in zip(captured, half_lives):
            k = Decimal(2).ln() / D(float(half))
            decay = (-k * minutes).exp()
            pressure = inspired + slope*(minutes - 1/k) - (inspired-D(initial)-slope/k)*decay
            values.append(float(pressure))
        return values


def integration_sample(c, model, module, gas, captured, pressure, seconds):
    if not seconds:
        return list(c['tissues_bar']), {'integration': 'captured_no_exposure'}
    # When all exponent arguments are tiny, upstream subtracts nearly equal
    # terms. Use the independently evaluated high-precision equation instead;
    # retain upstream output unmodified so the cancellation remains visible.
    short = seconds/60.0 * math.log(2)/min(model.N2_HALF_LIFE) < 1e-6
    provenance = {'integration': 'pinned_upstream', 'short_interval_fallback': short}
    try:
        state = model.load(c['start'], seconds/60.0, gas,
                           (pressure-c['start'])/(seconds/60.0), captured)
        raw = [pair[0] for pair in state.tissues]
    except (OverflowError, ZeroDivisionError, ValueError) as exc:
        if not short:
            raise
        raw = None
        provenance['upstream_error'] = {'type': type(exc).__name__, 'message': str(exc)}
    if short:
        provenance.update(integration='decimal_schreiner', decimal_precision=100,
                          raw_upstream_tissues_bar=None if raw is None else
                          [x if math.isfinite(x) else repr(x) for x in raw],
                          fallback_reason='all k*t < 1e-6; subtractive cancellation in upstream Schreiner equation')
        return decimal_schreiner(c['tissues_bar'],c['start'],pressure,seconds,model.N2_HALF_LIFE), provenance
    return raw, provenance


def evaluate(c, model, module, gas):
    result = {'status': 0, 'inspected_points': 0, 'grid': [], 'destination_valid': False,
              'diagnostic_valid': False, 'destination_nitrogen_bar': None}
    def finish(status):
        result['status'] = status
        return result
    surface, start, dest = (c[k] for k in ('surface','start','dest'))
    if (not all(math.isfinite(x) for x in (surface,start,dest)) or surface < .0627
            or start < dest or dest < surface):
        return finish(2)
    if c['mode'] == 'fixed':
        if not math.isfinite(c['gf']) or not 0 < c['gf'] <= 1:
            return finish(2)
    elif c.get('anchor') is not None and (not math.isfinite(c['anchor']) or c['anchor'] <= surface):
        return finish(2)
    if c.get('identity',0) or any(not math.isfinite(x) or x < 0 for x in c['tissues_bar']):
        return finish(3)
    duration = (start-dest)/RATE
    result['arrival_seconds'] = duration
    if not math.isfinite(duration):
        result['arrival_seconds'] = None
        return finish(4)
    if duration >= 2**53-1 or math.ceil(duration)+1 > c['allowance']:
        return finish(5)
    # Each sample is independently integrated from captured state at its exact
    # binary64 offset, never by calling any production transit/path helper.
    times = inspection_offsets(duration)
    captured = module.Data(tuple((p,0.0) for p in c['tissues_bar']), .70)
    for t in times:
        pressure = dest if t == duration else start-RATE*t
        gf = gf_at(c, pressure)
        try:
            values, provenance = integration_sample(c,model,module,gas,captured,pressure,t)
            if any(not math.isfinite(x) or x < 0 for x in values):
                return finish(4)
            # Upstream's mixed-gas helper divides by zero for zero inert gas.
            # Pure N2 zero compartment has this direct analytic limiting value.
            limits = [(-gf*a)/(1-gf+gf/b) if p == 0 else limit
                      for p,a,b,limit in zip(values,model.N2_A,model.N2_B,
                        model.gf_limit(gf,module.Data(tuple((max(p,1e-300),0.0) for p in values),gf)))]
            ceiling = max(surface,max(limits))
            if not all(math.isfinite(x) for x in limits+[ceiling]):
                return finish(4)
        except (OverflowError, ZeroDivisionError, ValueError):
            return finish(4)
        result['inspected_points'] += 1
        point = {'seconds':t,'pressure_bar_abs':pressure,'gf':gf,'ceiling_bar_abs':ceiling,
                 'margin_bar':pressure-ceiling,'tissues_bar':values,'numerical_provenance':provenance}
        result['grid'].append(point)
        if pressure+1e-9 < ceiling:
            result['diagnostic_valid'] = True
            for field, value in [('seconds',t),('pressure_bar_abs',pressure),('gf',gf),('ceiling_bar_abs',ceiling),('margin_bar',pressure-ceiling)]:
                result['failure_'+field] = value
            return finish(1)
    result['destination_valid'] = True
    result['destination_nitrogen_bar'] = result['grid'][-1]['tissues_bar']
    return result


def worker(archive, cases):
    validate(cases)
    source=verify(Path(archive))
    with tempfile.TemporaryDirectory(prefix='transit-reference-') as temp:
        with tarfile.open(fileobj=io.BytesIO(source), mode='r:gz') as bundle:
            bundle.extractall(temp, filter='data')
        root=Path(temp)/'decotengu-0.14.1'
        if not (root/'COPYING').is_file():
            raise ValueError('Upstream COPYING absent')
        sys.path.insert(0,str(root))
        module=importlib.import_module('decotengu.model')
        from decotengu.engine import GasMix
        model=module.ZH_L16C_GF()
        model.water_vapour_pressure=.0627
        return {'results':[evaluate(c,model,module,GasMix(0,21,79,0)) for c in cases],
                'coefficients':list(zip(model.N2_HALF_LIFE,model.N2_A,model.N2_B)),
                'archive_sha256':hashlib.sha256(source).hexdigest(),
                'upstream_model_sha256':hashlib.sha256((root/'decotengu/model.py').read_bytes()).hexdigest()}


def reference(archive,cases):
    validate(cases)
    verify(Path(archive))
    child=subprocess.run([sys.executable,'-I',str(Path(__file__).resolve()),'_worker','--archive',str(Path(archive).resolve())],
                         input=json.dumps(cases),text=True,capture_output=True,check=True)
    data=json.loads(child.stdout)
    if len(data['results']) != len(cases):
        raise AssertionError('Reference must return every requested transit')
    return data


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('command',choices=['calculate','_worker'])
    p.add_argument('--archive',type=Path,required=True)
    a=p.parse_args();cases=json.load(sys.stdin)
    print(json.dumps(worker(a.archive,cases) if a.command=='_worker' else reference(a.archive,cases),allow_nan=False))

if __name__=='__main__':
    main()
