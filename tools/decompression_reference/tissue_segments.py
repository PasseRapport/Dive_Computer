#!/usr/bin/env python3
"""Offline, hash-verified DecoTengu raw-segment reference; pressures bar absolute, time ms.

The isolated child alone imports the temporary verified upstream source. This
adapter does not import, read, or execute the production numerical implementation.
"""
import argparse
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
from runner import MANIFEST, ReferenceError, verify


def validate(request):
    surface = request['surface_bar']
    if not math.isfinite(surface) or surface < 0.0627:
        raise ValueError('surface_bar must be finite and >= 0.0627')
    for start, finish, duration in request['segments']:
        if any(not math.isfinite(v) or v < 0.0627 for v in (start, finish)):
            raise ValueError('absolute segment pressures must be finite and >= 0.0627')
        if type(duration) is not int or not 0 < duration <= 2**64 - 1:
            raise ValueError('duration must be positive uint64 milliseconds')


def worker(archive, request):
    validate(request)
    source = verify(archive)
    with tempfile.TemporaryDirectory(prefix='tissue-reference-') as temp:
        with tarfile.open(fileobj=io.BytesIO(source), mode='r:gz') as bundle:
            bundle.extractall(temp, filter='data')
        root = Path(temp) / 'decotengu-0.14.1'
        if not (root / 'COPYING').is_file():
            raise ReferenceError('dependency_missing', 'Upstream COPYING absent')
        sys.path.insert(0, str(root))
        module = importlib.import_module('decotengu.model')
        from decotengu.engine import GasMix
        model = module.ZH_L16C_GF()
        model.START_P_N2 = 0.79
        model.START_P_HE = 0.0
        model.water_vapour_pressure = 0.0627
        # Explicit test-only negative control. Never changes the cached archive.
        mutation = request.get('mutation')
        if mutation == 'half_time':
            half = list(model.N2_HALF_LIFE)
            half[0] *= 1.01
            model.N2_HALF_LIFE = tuple(half)
            model.n2_k_const = model._k_const(model.N2_HALF_LIFE)
        elif mutation is not None:
            raise ValueError('unknown test mutation')
        gas = GasMix(0, 21, 79, 0)
        state = model.init(request['surface_bar'])
        states = [[pair[0] for pair in state.tissues]]
        for start, finish, duration in request['segments']:
            minutes = duration / 60000.0
            state = model.load(start, minutes, gas, (finish - start) / minutes, state)
            values = [pair[0] for pair in state.tissues]
            if len(values) != 16 or not all(math.isfinite(v) for v in values):
                raise ReferenceError('nonfinite_tissues', 'Expected 16 finite nitrogen pressures')
            states.append(values)
        return {'states_bar': states,
                'coefficients': list(zip(model.N2_HALF_LIFE, model.N2_A, model.N2_B)),
                'archive_sha256': hashlib.sha256(source).hexdigest(),
                'upstream_model_sha256': hashlib.sha256((root / 'decotengu/model.py').read_bytes()).hexdigest(),
                'mutation': mutation}


def reference(archive, surface, segments, mutation=None):
    request = {'surface_bar': surface, 'segments': segments, 'mutation': mutation}
    validate(request)
    verify(Path(archive))
    child = subprocess.run([sys.executable, '-I', str(Path(__file__).resolve()),
                            '_worker', '--archive', str(Path(archive).resolve())],
                           input=json.dumps(request, allow_nan=False), text=True,
                           capture_output=True, check=True)
    return json.loads(child.stdout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['calculate', '_worker'])
    parser.add_argument('--archive', required=True, type=Path)
    args = parser.parse_args()
    request = json.load(sys.stdin)
    if args.command == '_worker':
        result = worker(args.archive, request)
    else:
        result = reference(args.archive, request['surface_bar'], request['segments'])
    print(json.dumps(result, allow_nan=False))


if __name__ == '__main__':
    main()
