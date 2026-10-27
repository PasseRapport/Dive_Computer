#!/usr/bin/env python3
"""Pinned isolated host smoke. Run --help for explicit acquisition and offline use."""
import argparse
import hashlib
import importlib
import io
import json
import math
from pathlib import Path
import platform
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

HERE = Path(__file__).resolve().parent
MANIFEST = json.loads((HERE / 'manifest.json').read_text())


class ReferenceError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


def verify(path):
    try:
        data = path.read_bytes()
    except FileNotFoundError as exc:
        raise ReferenceError('archive_missing', str(path)) from exc
    digest = hashlib.sha256(data).hexdigest()
    if digest != MANIFEST['dependency']['sha256']:
        raise ReferenceError('archive_hash_mismatch', 'Archive rejected before extraction or execution: ' + digest)
    return data


def calculate(root):
    # Only a fresh isolated child imports the hash-verified, temporary source.
    sys.path.insert(0, str(root))
    model_module = importlib.import_module('decotengu.model')
    from decotengu.engine import GasMix
    p = MANIFEST['policy']
    if p['model'] != 'ZH_L16C_GF' or p['gf_high'] != 0.70 or p['depth_m'] != 30.0:
        raise ReferenceError('model_policy_mismatch', 'This fixed smoke requires ZH_L16C_GF, GF70 and 30 m')
    model = model_module.ZH_L16C_GF()
    model.START_P_N2 = p['start_p_n2']
    model.START_P_HE = p['start_p_he_bar']
    model.water_vapour_pressure = p['water_vapour_pressure_bar']
    model.gf_low, model.gf_high = p['gf_low'], p['gf_high']
    if model.N2_HALF_LIFE[0] != p['first_n2_half_life_min']:
        raise ReferenceError('model_policy_mismatch', 'Expected four-minute first nitrogen compartment')
    pressure = p['surface_pressure_bar'] + p['density_kg_m3'] * p['gravity_m_s2'] * p['depth_m'] / 100000
    gas = GasMix(0, **p['gas_percent'])

    def once():
        initial = model.init(p['surface_pressure_bar'])
        final = model.load(pressure, p['duration_min'], gas, 0.0, initial)
        ceiling = model.ceiling_limit(final, gf=p['gf_high'])
        for tissues in (initial.tissues, final.tissues):
            if len(tissues) != 16 or any(len(t) != 2 or not all(math.isfinite(v) for v in t) for t in tissues):
                raise ReferenceError('nonfinite_tissues', 'Expected 16 finite nitrogen/helium pairs')
        if not math.isfinite(ceiling):
            raise ReferenceError('nonfinite_ceiling', 'Ceiling must be finite')
        return {'initial_tissues_bar': initial.tissues, 'final_tissues_bar': final.tissues,
                'ceiling_absolute_pressure_bar_gf70': ceiling}

    first, second = once(), once()
    if first != second:
        raise ReferenceError('repeat_mismatch', 'Repeated calculation differs')
    return dict(first, repeated_numerical_identity=True, ambient_pressure_bar=pressure,
                fixture='synthetic instantaneous exposure at 30 m from surface equilibrium; no descent/ascent or planner')


def verified_calculation(path):
    data = verify(path)
    with tempfile.TemporaryDirectory(prefix='decompression-reference-') as temp:
        root = Path(temp)
        with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
            # Verified source and its original notices/COPYING stay together.
            archive.extractall(root, filter='data')
        package = root / 'decotengu-0.14.1'
        if not (package / 'COPYING').is_file() or not (package / 'decotengu/model.py').is_file():
            raise ReferenceError('dependency_missing', 'Source or GPL COPYING absent')
        return calculate(package)


def smoke(path):
    verify(path)
    child = subprocess.run([sys.executable, '-I', str(Path(__file__).resolve()), '_worker', str(path.resolve())],
                           capture_output=True, text=True)
    if child.returncode:
        try:
            failure = json.loads(child.stdout)
            raise ReferenceError(failure['error']['code'], failure['error']['message'])
        except (ValueError, KeyError):
            raise ReferenceError('dependency_execution_failed', child.stderr.strip())
    return json.loads(child.stdout)


def same_file(first, second):
    if first.resolve() == second.resolve():
        return True
    return first.exists() and second.exists() and first.samefile(second)


def main():
    parser = argparse.ArgumentParser(description=__doc__, epilog=MANIFEST['evidence_scope'])
    sub = parser.add_subparsers(dest='command', required=True)
    for name in ('fetch', 'smoke'):
        cmd = sub.add_parser(name, help='explicit network acquisition' if name == 'fetch' else 'offline dependency smoke')
        cmd.add_argument('--archive', type=Path, required=True)
        cmd.add_argument('--report', type=Path)
    worker = sub.add_parser('_worker', help=argparse.SUPPRESS)
    worker.add_argument('archive', type=Path)
    args = parser.parse_args()
    result = {'report_schema': 1, 'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'manifest_sha256': hashlib.sha256((HERE / 'manifest.json').read_bytes()).hexdigest(),
              'status': 'ok', 'evidence_scope': MANIFEST['evidence_scope'],
              'dependency': MANIFEST['dependency'], 'inputs_and_constants': MANIFEST['policy'],
              'units': {'pressure': 'bar absolute', 'time': 'min', 'tissue_columns': ['nitrogen', 'helium']},
              'environment': {'python': sys.version, 'platform': platform.platform()}}
    code = 0
    try:
        if getattr(args, 'report', None) and same_file(args.archive, args.report):
            args.report = None  # Never overwrite the input while reporting this error.
            raise ReferenceError('path_conflict', 'Archive and report must be different files')
        if args.command == '_worker':
            result['result'] = verified_calculation(args.archive)
        elif args.command == 'smoke':
            result = smoke(args.archive)
        else:
            with urllib.request.urlopen(MANIFEST['dependency']['url'], timeout=60) as response:
                data = response.read()
            if hashlib.sha256(data).hexdigest() != MANIFEST['dependency']['sha256']:
                raise ReferenceError('archive_hash_mismatch', 'Downloaded archive rejected; destination unchanged')
            args.archive.parent.mkdir(parents=True, exist_ok=True)
            args.archive.write_bytes(data)
            result['archive'] = str(args.archive.resolve())
    except Exception as exc:
        code = 1
        result.update(status='error', error={'code': getattr(exc, 'code', 'dependency_missing' if isinstance(exc, ImportError) else 'execution_failed'), 'message': str(exc)})
    rendered = json.dumps(result, indent=2, allow_nan=False) + '\n'
    if getattr(args, 'report', None):
        try:
            args.report.write_text(rendered)
        except OSError as exc:
            result.update(status='error', error={'code': 'report_write_failed', 'message': str(exc)})
            rendered, code = json.dumps(result, indent=2) + '\n', 1
    print(rendered, end='')
    return code


if __name__ == '__main__':
    sys.exit(main())
