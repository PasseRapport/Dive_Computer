#!/usr/bin/env python3
"""Compare all C nitrogen compartments to isolated, verified DecoTengu.

Run directly with --cli, --archive, --build-dir and --report for full evidence.
Unittest discovery uses DECO_TISSUE_CLI and DECO_REFERENCE_ARCHIVE; absent inputs
are explicit skips rather than claimed reference passes.
"""
import argparse
import copy
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import tissue_segments
from tissue_segments import reference

REPO = HERE.parents[1]
ABS_TOL = REL_TOL = 1e-7
SURFACE = 1.01325
POLICY_ID = 'air-zhl16c-4min-gf30-70-v1'
FIXTURES = {
    'surface_equilibrium': [[SURFACE, SURFACE, 3600000]],
    'hold_loading_and_elimination': [[4.01325, 4.01325, 240000],
                                     [SURFACE, SURFACE, 240000],
                                     [SURFACE, SURFACE, 3600000]],
    'descent_hold_ascent_surface': [[SURFACE, 4.01325, 90000],
                                  [4.01325, 4.01325, 2100000],
                                  [4.01325, SURFACE, 180000],
                                  [SURFACE, SURFACE, 7200000]],
    'short_intervals': [[SURFACE, 1.01326, 1], [1.01326, 1.01326, 1],
                        [1.01326, SURFACE, 1], [4.01325, 4.01325, 1]],
    'one_second_ramps': [[SURFACE, 4.01325, 1000], [4.01325, SURFACE, 1000]],
    'partition_whole': [[SURFACE, 4.01325, 180000]],
    'partition_split': [[SURFACE + i * 0.3, SURFACE + (i + 1) * 0.3, 18000]
                        for i in range(10)],
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def compare(actual, expected):
    if len(actual) != 16 or len(expected) != 16:
        raise AssertionError('Expected exactly 16 compartments')
    errors = []
    for i, (a, e) in enumerate(zip(actual, expected), 1):
        limit = ABS_TOL + REL_TOL * abs(e)
        error = abs(a - e)
        errors.append({'compartment': i, 'actual_bar': a, 'reference_bar': e,
                       'absolute_error_bar': error, 'tolerance_bar': limit,
                       'pass': math.isfinite(a) and math.isfinite(e) and error <= limit})
    return errors


def compiled(cli, surface, segments):
    child = subprocess.run([str(Path(cli).resolve()), repr(surface)],
                           input=''.join(f'{s!r} {e!r} {t}\n' for s, e, t in segments),
                           capture_output=True, text=True, check=True)
    rows = [json.loads(line) for line in child.stdout.splitlines()]
    if len(rows) != len(segments) + 2 or any(row['result'] != 0 for row in rows[1:]):
        raise AssertionError('CLI output length or result status invalid')
    return rows[0], [row['nitrogen_bar'] for row in rows[1:]], child.stdout


def table_gate(constants, table, reference_coefficients):
    expected = [[float(row[k]) for k in ('half_life', 'a', 'b')] for row in table['rows']]
    if constants['coefficients'] != expected or reference_coefficients != expected:
        raise AssertionError('Compiled/reference/accepted coefficient tables differ')
    if constants['coefficient_id'] != table['policy_id']:
        raise AssertionError('Coefficient identity mismatch')
    if constants['coefficient_sha256'] != digest(HERE / 'nitrogen_coefficients.v1.json'):
        raise AssertionError('Coefficient manifest digest mismatch')
    if (constants['policy_id'] != POLICY_ID or constants['nitrogen_fraction'] != 0.79
            or constants['water_vapour_bar'] != 0.0627):
        raise AssertionError('Policy mismatch')


def require_pass(errors):
    if not all(item['pass'] for item in errors):
        raise AssertionError('Tissue mismatch: ' + json.dumps([e for e in errors if not e['pass']]))


def reference_states(expected, segments):
    states = expected['states_bar']
    if len(states) != len(segments) + 1:
        raise AssertionError('Reference must return initial state and every segment state')
    return states


def exercise(cli, archive):
    table = json.loads((HERE / 'nitrogen_coefficients.v1.json').read_text())
    fixtures = []
    results = {}
    for name, segments in FIXTURES.items():
        expected = reference(archive, SURFACE, segments)
        expected_states = reference_states(expected, segments)
        constants, states, raw = compiled(cli, SURFACE, segments)
        table_gate(constants, table, expected['coefficients'])
        if compiled(cli, SURFACE, segments)[2] != raw:
            raise AssertionError('Identical input produced different executable output')
        comparisons = [compare(a, e) for a, e in zip(states, expected_states)]
        for errors in comparisons:
            require_pass(errors)
        fixtures.append({'name': name, 'surface_bar': SURFACE, 'segments': segments,
                         'states': comparisons, 'repeat_byte_identity': True})
        results[name] = states
    # Independently expected equilibrium and one half-time uptake, not library calls.
    initial = 0.79 * (SURFACE - 0.0627)
    require_pass(compare(results['surface_equilibrium'][-1], [initial] * 16))
    half_times = [float(row['half_life']) for row in table['rows']]
    inspired = 0.79 * (4.01325 - 0.0627)
    analytical = [initial + (inspired - initial) * (1 - 2**(-4 / h)) for h in half_times]
    analytical_errors = compare(results['hold_loading_and_elimination'][1], analytical)
    require_pass(analytical_errors)
    partition_errors = compare(results['partition_split'][-1], results['partition_whole'][-1])
    require_pass(partition_errors)
    mutation_segments = [[4.01325, 4.01325, 240000]]
    expected = reference(archive, SURFACE, mutation_segments)
    expected_states = reference_states(expected, mutation_segments)
    constants, normal, _ = compiled(cli, SURFACE, mutation_segments)
    _, wrong_time, _ = compiled(cli, SURFACE, [[4.01325, 4.01325, 4000]])
    time_errors = compare(wrong_time[-1], expected_states[-1])
    wrong_half = reference(archive, SURFACE, mutation_segments, mutation='half_time')
    half_errors = compare(normal[-1], reference_states(wrong_half, mutation_segments)[-1])
    if all(e['pass'] for e in time_errors) or all(e['pass'] for e in half_errors):
        raise AssertionError('Negative control was not detected at unchanged tolerance')
    mutated_table = copy.deepcopy(constants)
    mutated_table['coefficients'][0][0] *= 1.01
    try:
        table_gate(mutated_table, table, expected['coefficients'])
    except AssertionError:
        table_mutation_detected = True
    else:
        raise AssertionError('Table mutation was not detected')
    return {'status': 'pass', 'scope': 'host tissue integration only; no physiological, target or real-dive validation',
            'tolerance': {'absolute_bar': ABS_TOL, 'relative': REL_TOL},
            'policy_id': constants['policy_id'], 'compiled_constants': constants,
            'policy_sha256': hashlib.sha256(json.dumps({'policy_id': POLICY_ID, 'nitrogen_fraction': 0.79, 'water_vapour_bar': 0.0627, 'duration_unit': 'milliseconds', 'pressure_unit': 'bar absolute'}, sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
            'table_equality': True, 'reference_identity': {k: expected[k] for k in ('archive_sha256', 'upstream_model_sha256')},
            'fixtures': fixtures, 'analytical_hold_errors': analytical_errors,
            'partition_errors': partition_errors,
            'mutation_controls': {'description': 'Wrong time is CLI duration divided by 60; half-time perturbed +1% in isolated reference; compiled table copy perturbed +1%. No production source mutation.',
                                  'wrong_time_errors': time_errors, 'half_time_errors': half_errors,
                                  'table_mutation_detected': table_mutation_detected}}


def configured_compiler(build_dir):
    for line in (build_dir / 'CMakeCache.txt').read_text().splitlines():
        key, separator, value = line.partition('=')
        if separator and key.startswith('CMAKE_C_COMPILER:'):
            compiler = Path(value)
            if not compiler.is_absolute() or not compiler.is_file():
                raise ValueError('Configured C compiler must be an existing absolute path')
            return compiler
    raise ValueError('CMAKE_C_COMPILER is missing from the supplied build cache')


def evidence_inputs(cli, archive, build_dir):
    # Construct the entire input inventory before any execution or report write.
    files = [HERE / n for n in ('tissue_segments.py', 'test_tissue_integration.py',
                               'runner.py', 'manifest.json', 'nitrogen_coefficients.v1.json')]
    core = REPO / 'V2/Firmware/deco_core'
    for pattern in ('*.c', '*.h', '*.cmake'):
        files += sorted(core.rglob(pattern))
    return files + [core / 'CMakeLists.txt', cli, archive,
                    build_dir / 'CMakeCache.txt', build_dir / 'compile_commands.json']


def prepare_evidence(cli, archive, build_dir, report):
    files = evidence_inputs(cli, archive, build_dir)
    if cli.resolve() != (build_dir / 'tissue_segments_cli').resolve():
        raise ValueError('CLI must be tissue_segments_cli from the supplied build directory')
    compiler = configured_compiler(build_dir)
    protected = files + [compiler]
    if any(report.resolve() == path.resolve() or
           (report.exists() and path.exists() and report.samefile(path))
           for path in protected):
        raise ValueError('report must not overwrite or alias any evidence input')
    return files, compiler


class TissueIntegrationTests(unittest.TestCase):
    def test_reference_rejects_empty_or_truncated_states_before_comparison(self):
        for states in ([], [[1.0] * 16]):
            with self.subTest(states=len(states)), \
                    patch(__name__ + '.reference', return_value={'states_bar': states}), \
                    patch(__name__ + '.compiled') as execute:
                with self.assertRaisesRegex(AssertionError, 'Reference must return'):
                    exercise('unused-cli', 'unused-archive')
                execute.assert_not_called()

    def test_report_cannot_alias_any_evidence_input(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / 'build'
            build.mkdir()
            cli = build / 'tissue_segments_cli'
            cli.write_text('cli')
            compiler = root / 'actual-compiler'
            compiler.write_text('compiler')
            cache = build / 'CMakeCache.txt'
            cache.write_text(f'CMAKE_C_COMPILER:FILEPATH={compiler}\n')
            commands = build / 'compile_commands.json'
            commands.write_text('[]')
            inputs = [root / name for name in ('source.c', 'header.h', 'test.py',
                                              'manifest.json', 'archive.tar.gz')]
            for path in inputs:
                path.write_text('evidence')
            inputs += [cli, cache, commands]
            with patch(__name__ + '.evidence_inputs', return_value=inputs):
                for path in inputs + [compiler]:
                    with self.subTest(path=path.name):
                        for alias in ('direct', 'symlink', 'hardlink'):
                            report = root / ('report-' + alias)
                            if alias == 'direct':
                                report = path
                            elif alias == 'symlink':
                                report.symlink_to(path)
                            else:
                                report.hardlink_to(path)
                            with self.assertRaisesRegex(ValueError, 'evidence input'):
                                prepare_evidence(cli, inputs[4], build, report)
                            if alias != 'direct':
                                report.unlink()

    def test_compiler_is_read_from_cache_and_mismatched_build_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            build = root / 'build'
            build.mkdir()
            compiler = root / 'configured-compiler'
            compiler.write_text('compiler')
            (build / 'CMakeCache.txt').write_text(f'CMAKE_C_COMPILER:FILEPATH={compiler}\n')
            cli = build / 'tissue_segments_cli'
            self.assertEqual(configured_compiler(build), compiler)
            self.assertEqual(prepare_evidence(cli, root / 'archive', build,
                                             root / 'report')[1], compiler)
            with self.assertRaisesRegex(ValueError, 'supplied build directory'):
                prepare_evidence(root / 'other-build/tissue_segments_cli', root / 'archive',
                                 build, root / 'report')

    def test_comparison_and_mutation_controls(self):
        cli = os.environ.get('DECO_TISSUE_CLI')
        archive = os.environ.get('DECO_REFERENCE_ARCHIVE')
        if not cli or not archive:
            self.skipTest('Set DECO_TISSUE_CLI and DECO_REFERENCE_ARCHIVE for real comparison')
        self.assertEqual(exercise(cli, archive)['status'], 'pass')

    def test_archive_rejected_before_extraction_or_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            archive = Path(temp) / 'unverified.tar.gz'
            archive.write_bytes(b'not the pinned source')
            with patch.object(tissue_segments.subprocess, 'run') as execute:
                with self.assertRaises(tissue_segments.ReferenceError):
                    reference(archive, SURFACE, [])
                execute.assert_not_called()
            with patch.object(tissue_segments.tarfile, 'open') as extract:
                with self.assertRaises(tissue_segments.ReferenceError):
                    tissue_segments.worker(archive, {'surface_bar': SURFACE, 'segments': []})
                extract.assert_not_called()

    def test_invalid_reference_requests_rejected(self):
        for segment in ([1.0, 2.0, 0], [1.0, 2.0, True], [1.0, 2.0, 1.5],
                        [1.0, 2.0, 2**64], [float('nan'), 1.0, 1], [0.0, 1.0, 1]):
            with self.subTest(segment=segment), self.assertRaises(ValueError):
                tissue_segments.validate({'surface_bar': SURFACE, 'segments': [segment]})

    def test_comparator_rejects_nonfinite_and_missing_compartments(self):
        self.assertFalse(compare([float('nan')] * 16, [1.0] * 16)[0]['pass'])
        with self.assertRaises(AssertionError):
            compare([1.0], [1.0] * 16)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cli', required=True, type=Path)
    parser.add_argument('--archive', required=True, type=Path)
    parser.add_argument('--build-dir', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    try:
        files, compiler = prepare_evidence(args.cli, args.archive, args.build_dir, args.report)
    except ValueError as error:
        parser.error(str(error))
    report = exercise(args.cli, args.archive)
    # Production sources are fingerprinted as opaque bytes, never parsed or imported.
    report['file_sha256'] = {str(path.resolve()): digest(path) for path in files}
    report['environment'] = {'python': sys.version, 'platform': platform.platform(),
                             'compiler_path': str(compiler.resolve()),
                             'compiler': subprocess.check_output([str(compiler), '--version'], text=True).splitlines()[0],
                             'compile_commands': json.loads((args.build_dir / 'compile_commands.json').read_text()),
                             'git_head': subprocess.check_output(['git', '-C', str(REPO), 'rev-parse', 'HEAD'], text=True).strip()}
    report['command'] = [sys.executable] + sys.argv
    args.report.write_text(json.dumps(report, indent=2, allow_nan=False) + '\n')
    print(json.dumps({'status': report['status'], 'fixtures': len(report['fixtures']),
                      'report': str(args.report.resolve())}))


if __name__ == '__main__':
    main()
