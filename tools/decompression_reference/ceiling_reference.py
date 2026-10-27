#!/usr/bin/env python3
"""Independent offline ceiling oracle. Only isolated child imports pinned upstream."""
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
from runner import verify, ReferenceError


def validate(request):
    if not isinstance(request, list) or not request:
        raise ValueError('Expected nonempty cases')
    for case in request:
        tissues, gf = case['tissues_bar'], case['gf']
        # Upstream mixed-gas implementation divides by total inert pressure;
        # zero-nitrogen states are tested analytically in the harness instead.
        if len(tissues) != 16 or any(not math.isfinite(p) or p <= 0 for p in tissues):
            raise ValueError('Expected 16 finite strictly positive nitrogen pressures')
        if not math.isfinite(gf) or not 0 < gf <= 1:
            raise ValueError('GF must be in (0, 1]')


def worker(archive, request):
    validate(request)
    source = verify(Path(archive))
    with tempfile.TemporaryDirectory(prefix='ceiling-reference-') as temp:
        with tarfile.open(fileobj=io.BytesIO(source), mode='r:gz') as bundle:
            bundle.extractall(temp, filter='data')
        root = Path(temp) / 'decotengu-0.14.1'
        if not (root / 'COPYING').is_file():
            raise ReferenceError('dependency_missing', 'Upstream COPYING absent')
        sys.path.insert(0, str(root))
        module = importlib.import_module('decotengu.model')
        model = module.ZH_L16C_GF()
        results = []
        for case in request:
            data = module.Data(tuple((p, 0.0) for p in case['tissues_bar']), case['gf'])
            limits = list(model.gf_limit(case['gf'], data))
            maximum = model.ceiling_limit(data, gf=case['gf'])
            if len(limits) != 16 or not all(math.isfinite(p) for p in limits + [maximum]):
                raise ReferenceError('nonfinite_ceiling', 'Expected 16 finite limits and maximum')
            results.append({'raw_limits_bar_abs': limits, 'raw_max_bar_abs': maximum})
        return {'results': results, 'coefficients': list(zip(model.N2_HALF_LIFE, model.N2_A, model.N2_B)),
                'archive_sha256': hashlib.sha256(source).hexdigest(),
                'upstream_model_sha256': hashlib.sha256((root / 'decotengu/model.py').read_bytes()).hexdigest()}


def reference(archive, request):
    validate(request)
    verify(Path(archive))
    child = subprocess.run([sys.executable, '-I', str(Path(__file__).resolve()),
                            '_worker', '--archive', str(Path(archive).resolve())],
                           input=json.dumps(request, allow_nan=False), text=True,
                           capture_output=True, check=True)
    result = json.loads(child.stdout)
    if len(result['results']) != len(request):
        raise AssertionError('Reference must return every requested ceiling')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command', choices=['calculate', '_worker'])
    parser.add_argument('--archive', required=True, type=Path)
    args = parser.parse_args()
    request = json.load(sys.stdin)
    result = worker(args.archive, request) if args.command == '_worker' else reference(args.archive, request)
    print(json.dumps(result, allow_nan=False))


if __name__ == '__main__':
    main()
