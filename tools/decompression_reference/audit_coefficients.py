#!/usr/bin/env python3
"""Offline exact-decimal provenance gate; never executes reference source."""
import argparse
import ast
from decimal import Decimal
import hashlib
import io
import json
from pathlib import Path
import re
import sys
import tarfile
sys.path.insert(0, str(Path(__file__).resolve().parent))
try:
    import runner
    RUNNER_ERROR = None
except Exception as exc:
    runner = None
    RUNNER_ERROR = str(exc)

HERE = Path(__file__).resolve().parent
SOURCE_SHA256 = '5f0cdc1e26bdcc6a79077ae71c050ba5751984c00f0184d22cce8eb138f5b116'
POLICY = 'zhl16c-n2-4min-table25-v1'
UNITS = {'half_life': 'min', 'a': 'bar', 'b': 'dimensionless'}
FIELDS = {'half_life': 'N2_HALF_LIFE', 'a': 'N2_A', 'b': 'N2_B'}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def strict_json(data):
    def pairs(items):
        result = {}
        for key, value in items:
            require(key not in result, 'duplicate JSON key: ' + key)
            result[key] = value
        return result
    return json.loads(data, object_pairs_hook=pairs)


def same_file(a, b):
    return a.resolve() == b.resolve() or (a.exists() and b.exists() and a.samefile(b))


def validate(table):
    require(type(table['schema_version']) is int and table['schema_version'] == 1 and table['model'] == 'ZH_L16C_GF'
            and table['gas'] == 'N2' and table['policy_id'] == POLICY, 'schema/model/policy mismatch')
    require(table['units'] == UNITS, 'units mismatch')
    rows = table['rows']
    require(len(rows) == 16 and all(type(r['compartment']) is int for r in rows) and [r['compartment'] for r in rows] == list(range(1, 17)), 'row count/order mismatch')
    for row in rows:
        require(set(row) == {'compartment', *FIELDS}, 'row schema mismatch')
        for key in FIELDS:
            require(isinstance(row[key], str) and re.fullmatch(r'\d+\.\d+', row[key]) is not None,
                    'coefficients must be decimal strings')
            require(len(row[key].split('.')[1]) == (1 if key == 'half_life' else 4), 'printed precision mismatch')
            require(Decimal(row[key]).is_finite() and Decimal(row[key]) > 0, 'invalid coefficient')
    require([rows[0][k] for k in FIELDS] == ['4.0', '1.2599', '0.5050'], 'four-minute policy mismatch')


def reference_constants(data):
    with tarfile.open(fileobj=io.BytesIO(data), mode='r:gz') as archive:
        member = archive.extractfile('decotengu-0.14.1/decotengu/model.py')
        require(member is not None, 'reference source missing')
        source = member.read().decode('utf-8')
    classes = [node for node in ast.parse(source).body if isinstance(node, ast.ClassDef) and node.name == 'ZH_L16C_GF']
    require(len(classes) == 1, 'reference model missing or ambiguous')
    values = {}
    for node in classes[0].body:
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in FIELDS.values():
                    require(target.id not in values and isinstance(node.value, ast.Tuple), 'reference declaration mismatch')
                    lexemes = [ast.get_source_segment(source, item) for item in node.value.elts]
                    require(len(lexemes) == 16 and all(re.fullmatch(r'\d+\.\d+', s or '') for s in lexemes), 'reference values invalid')
                    values[target.id] = lexemes
    require(set(values) == set(FIELDS.values()), 'reference constants incomplete')
    return values, hashlib.sha256(source.encode()).hexdigest()


def audit(archive, source, manifest, transcription):
    result = {'report_schema': 1, 'status': 'error', 'policy_id': POLICY,
              'scope': 'exact coefficient comparison; documentary acceptance requires separate critical review',
              'critical_review': 'not_evaluated_by_this_tool',
              'checks': [], 'errors': [], 'hashes': {}}
    try:
        require(RUNNER_ERROR is None, 'dependency initialization failed: ' + str(RUNNER_ERROR))
        data = runner.verify(archive)
        source_bytes, manifest_bytes, printed_bytes = source.read_bytes(), manifest.read_bytes(), transcription.read_bytes()
        blobs = {'archive': data, 'source': source_bytes, 'manifest': manifest_bytes, 'transcription': printed_bytes, 'auditor': Path(__file__).read_bytes(), 'runner': (HERE / 'runner.py').read_bytes()}
        result['hashes'] = {label: hashlib.sha256(blob).hexdigest() for label, blob in blobs.items()}
        require(result['hashes']['source'] == SOURCE_SHA256, 'source hash mismatch')
        candidate = strict_json(manifest_bytes)
        printed = strict_json(printed_bytes)
        validate(candidate)
        validate(printed)
        provenance = printed['source']
        require(provenance['sha256'] == SOURCE_SHA256 and provenance['url'] == 'https://www.cmas.org/images/2025/08/09/appendix-i.webp'
                and provenance['page'] == 158 and provenance['table'] == 25, 'source metadata mismatch')
        require(provenance['columns'] == {'half_life': 't1/2 N2 [min]', 'a': 'ZH-L16C Computer a', 'b': 'common ZH-L16 b'}
                and provenance['row_selection'] == '1, 2–16; alternative row 1b excluded'
                and provenance['bibliography'] == 'Bühlmann A. A., Völlm E. B., Nussberger P., Tauchmedizin, Springer-Verlag, 2002', 'transcription provenance mismatch')
        require(candidate['source_reference'] == {'transcription': 'nitrogen_printed_table.v1.json', 'page': 158, 'table': 25,
                'row_mapping': 'compartment number maps to printed row; exclude 1b'}, 'manifest provenance mismatch')
        result['source'] = provenance
        result['units'] = UNITS
        result['reference'] = runner.MANIFEST['dependency']
        constants, result['hashes']['reference_model'] = reference_constants(data)
        for i in range(16):
            for key, name in FIELDS.items():
                values = [candidate['rows'][i][key], printed['rows'][i][key], constants[name][i]]
                matched = len(set(map(Decimal, values))) == 1
                result['checks'].append({'compartment': i + 1, 'field': key, 'manifest': values[0], 'printed': values[1], 'reference': values[2], 'match': matched})
        require(all(c['match'] for c in result['checks']), 'coefficient mismatch')
        result['status'] = 'pass'
    except Exception as exc:
        result['errors'].append({'code': getattr(exc, 'code', 'audit_failed'), 'message': str(exc)})
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('archive', 'source', 'report'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--manifest', type=Path, default=HERE / 'nitrogen_coefficients.v1.json')
    parser.add_argument('--transcription', type=Path, default=HERE / 'nitrogen_printed_table.v1.json')
    args = parser.parse_args()
    inputs = [args.archive, args.source, args.manifest, args.transcription, Path(__file__), HERE / 'runner.py', HERE / 'manifest.json']
    if any(same_file(args.report, path) for path in inputs):
        print(json.dumps({'report_schema': 1, 'status': 'error', 'checks': [], 'errors': [{'code': 'path_conflict', 'message': 'Report would overwrite an input'}]}))
        return 1
    result = audit(args.archive, args.source, args.manifest, args.transcription)
    try:
        args.report.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n')
    except OSError as exc:
        result['status'] = 'error'
        result['errors'].append({'code': 'report_write_failed', 'message': str(exc)})
    print(json.dumps(result, indent=2, allow_nan=False))
    return 0 if result['status'] == 'pass' else 1


if __name__ == '__main__':
    sys.exit(main())
