#!/usr/bin/env python3
"""Independent ceiling evidence; run with real CLI/archive/build inputs. No acceptance skips."""
import argparse
from decimal import Decimal
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
import ceiling_reference
import test_tissue_integration as tissue

ABS_TOL = 1e-6
GFS = (0.30, 0.45, 0.50, 0.70, 1.0, math.nextafter(0.0, 1.0), math.nextafter(1.0, 0.0))


def compiled(cli, commands):
    child = subprocess.run([str(Path(cli).resolve())], input=''.join(c + '\n' for c in commands),
                           text=True, capture_output=True, check=True)
    rows = [json.loads(line) for line in child.stdout.splitlines()]
    if len(rows) != len(commands) + 1:
        raise AssertionError('CLI must return identity and every requested result')
    return rows[0], rows[1:], child.stdout


def compare(actual, expected):
    if len(actual) != 16 or len(expected) != 16:
        raise AssertionError('Expected exactly 16 compartments')
    rows = []
    for i, (a, e) in enumerate(zip(actual, expected), 1):
        ok = a is not None and math.isfinite(a) and math.isfinite(e) and abs(a-e) <= ABS_TOL
        rows.append({'compartment': i, 'actual_bar': a, 'reference_bar': e,
                     'absolute_error_bar': abs(a-e) if a is not None else None, 'pass': ok})
    if not all(r['pass'] for r in rows):
        raise AssertionError('Ceiling compartment mismatch')
    return rows


def fixed_command(state, gf, surface=tissue.SURFACE, identity=0):
    return ' '.join(['fixed', repr(surface), repr(gf), str(identity)] + [repr(p) for p in state])


def exercise(cli, tissue_cli, archive):
    table = json.loads((HERE / 'nitrogen_coefficients.v1.json').read_text())
    cases, state_origins = [], []
    for name, segments in tissue.FIXTURES.items():
        upstream = tissue.reference(archive, tissue.SURFACE, segments)
        reference_states = tissue.reference_states(upstream, segments)
        constants, states, _ = tissue.compiled(tissue_cli, tissue.SURFACE, segments)
        tissue.table_gate(constants, table, upstream['coefficients'])
        if len(states) != len(segments) + 1:
            raise AssertionError('C tissue adapter must return every state')
        for origin, values in [('story_1_3_C', states), ('isolated_reference', reference_states)]:
            for step, state in enumerate(values):
                state_origins.append({'fixture': name, 'origin': origin, 'step': step, 'tissues_bar': state})
    # Invert the stated constraint independently to construct equal limits and
    # a distinct controlling compartment for every index and GF.
    for gf in GFS:
        equal = [2.0 * (1 - gf + gf / float(r['b'])) + gf * float(r['a']) for r in table['rows']]
        cases.append({'name': 'equal_limits', 'origin': 'controlled', 'gf': gf, 'tissues_bar': equal})
        for controller in range(16):
            state = [0.1] * 16
            state[controller] = equal[controller]
            cases.append({'name': f'controller_{controller+1}', 'origin': 'controlled', 'gf': gf, 'tissues_bar': state})
    for state in state_origins:
        for gf in GFS:
            cases.append(dict(state, name=state['fixture'], gf=gf))
    # Surface clamping is independently checked at other supported surfaces.
    surface_states = (state_origins[0], max(state_origins, key=lambda item: max(item['tissues_bar'])))
    for surface in (0.8, 1.1):
        for state in surface_states:
            for gf in GFS:
                cases.append(dict(state, name='alternate_surface', surface_bar_abs=surface, gf=gf))
    expected_count = len(state_origins) * len(GFS) + 21 * len(GFS)
    if len(state_origins) != 2 * sum(len(s)+1 for s in tissue.FIXTURES.values()) or len(cases) != expected_count:
        raise AssertionError('Fixture/state comparison count mismatch')
    oracle = ceiling_reference.reference(archive, cases)
    if len(oracle['results']) != expected_count:
        raise AssertionError('Reference must return every requested ceiling')
    tissue.table_gate(constants, table, oracle['coefficients'])
    commands = [fixed_command(c['tissues_bar'], c['gf'], c.get('surface_bar_abs', tissue.SURFACE)) for c in cases]
    identity, actual, raw = compiled(cli, commands)
    if any(identity[k] != constants[k] for k in identity):
        raise AssertionError('Ceiling/tissue adapter identity mismatch')
    if compiled(cli, commands)[2] != raw:
        raise AssertionError('Repeated ceiling calls differ')
    comparisons = []
    for case, a, e in zip(cases, actual, oracle['results']):
        if a['status'] != 0 or not a['input_unchanged']:
            raise AssertionError('Valid result unavailable or inputs mutated')
        errors = compare(a['raw_limits_bar_abs'], e['raw_limits_bar_abs'])
        if any(not math.isfinite(a[k]) for k in ('raw_max_bar_abs', 'local_ceiling_bar_abs')) or abs(a['raw_max_bar_abs']-e['raw_max_bar_abs']) > ABS_TOL or abs(a['local_ceiling_bar_abs']-max(case.get('surface_bar_abs', tissue.SURFACE), e['raw_max_bar_abs'])) > ABS_TOL:
            raise AssertionError('Maximum/clamp mismatch')
        if case['name'].startswith('controller_'):
            if a['raw_limits_bar_abs'].index(max(a['raw_limits_bar_abs'])) != int(case['name'].split('_')[1])-1:
                raise AssertionError('Expected controlling compartment absent')
        if case['name'] == 'equal_limits' and any(abs(x-2.0) > ABS_TOL for x in a['raw_limits_bar_abs']):
            raise AssertionError('Equal-limit construction failed')
        comparisons.append({'input': case, 'actual': a, 'reference': e, 'errors': errors})
    gf_cases = []
    # Decimal pressure ratios derive the pressure-linear policy independently.
    for surface, anchor in [(1.0, 3.0), (0.0627, 0.5), (1.01325, 4.01325)]:
        for fraction in (-0.5, 0, 0.25, 0.5, 0.75, 1, 1.5):
            current = max(0.0627, surface + fraction*(anchor-surface))
            weight = (Decimal(str(current))-Decimal(str(surface)))/(Decimal(str(anchor))-Decimal(str(surface)))
            expected = float(Decimal('0.70') - Decimal('0.40') * min(Decimal(1), max(Decimal(0), weight)))
            gf_cases.append({'current': current, 'surface': surface, 'anchor': anchor, 'expected': expected})
        gf_cases.append({'current': anchor + 1, 'surface': surface, 'anchor': None, 'expected': 0.70})
    _, gf_rows, _ = compiled(cli, [f"gf {c['current']!r} {c['surface']!r} {c['anchor'] if c['anchor'] is not None else 'none'}" for c in gf_cases])
    for case, row in zip(gf_cases, gf_rows):
        if row['status'] != 0 or not row['input_unchanged'] or abs(row['gf']-case['expected']) > 2e-15:
            raise AssertionError('Independent interpolation mismatch')
        case['actual'] = row
    boundaries = []
    for current in (0.0627, 1.0, 2.0, 100.0):
        rounded_sum = current + 1e-9
        for label, ceiling in [('at_current',current), ('below_threshold',math.nextafter(rounded_sum,-math.inf)), ('at_threshold',rounded_sum), ('above_threshold',math.nextafter(rounded_sum, math.inf))]:
            boundaries.append({'label':label,'current_bar_abs':current,'local_ceiling_bar_abs':ceiling,
                               'current_hex':current.hex(),'rounded_sum_hex':rounded_sum.hex(),'ceiling_hex':ceiling.hex(),
                               'expected': rounded_sum < ceiling})
    _, boundary_rows, _ = compiled(cli,[f"violation {c['current_bar_abs']!r} 0 {c['local_ceiling_bar_abs']!r}" for c in boundaries])
    for case,row in zip(boundaries,boundary_rows):
        if row['status'] != 0 or row['violated'] != case['expected']:
            raise AssertionError('Representable violation boundary mismatch')
        case['actual']=row
    valid=fixed_command([1.0]*16,0.7)
    invalid_commands=[fixed_command([1.0]*16,g) for g in (0.0,-0.1,1.01,math.nan,math.inf)]
    invalid_commands += [fixed_command([1.0]*16,0.7,s) for s in (0.0,math.nan,math.inf)]
    invalid_commands += [fixed_command([1.0]*16,0.7,identity=i) for i in (1,2)]
    for i in range(16):
        for value in (-0.1,math.nan,math.inf):
            state=[1.0]*16;state[i]=value
            invalid_commands.append(fixed_command(state,0.7))
    invalid_commands += [f'gf {c} {s} {a}' for c,s,a in [('nan','1','none'),('inf','1','3'),('0','1','none'),('1','nan','none'),('1','0','3'),('1','1','nan'),('1','1','inf'),('1','1','1'),('1','1','0.5')]]
    invalid_commands += ['violation nan 0 2','violation inf 0 2','violation 0 0 2','violation 1 0 nan','violation 1 0 inf','violation 1 0 0']
    invalid_commands += [f'violation 1 {s} 2' for s in (1,2,3)]
    sequence=[command for invalid in invalid_commands for command in (valid,invalid)]
    _, invalid_rows, _=compiled(cli,sequence)
    invalid_evidence=[]
    for i,command in enumerate(invalid_commands):
        previous,row=invalid_rows[2*i:2*i+2]
        if previous['status'] != 0 or row['status'] == 0 or row.get('input_unchanged',True) is not True:
            raise AssertionError('Invalid after valid or input preservation failed')
        if any(v is not None for k,v in row.items() if k in ('gf','violated','raw_max_bar_abs','local_ceiling_bar_abs')) or any(v is not None for v in row.get('raw_limits_bar_abs',[])):
            raise AssertionError('Invalid result exposes physiological output')
        invalid_evidence.append({'request':command,'preceded_by_valid':True,'actual':row})
    # Zero nitrogen is valid; upstream mixed-gas helper is undefined at zero.
    zero=[-0.7*float(r['a'])/(1-0.7+0.7/float(r['b'])) for r in table['rows']]
    _, zero_rows,_=compiled(cli,[fixed_command([0.0]*16,0.7)])
    zero_errors=compare(zero_rows[0]['raw_limits_bar_abs'],zero)
    if zero_rows[0]['status'] != 0 or zero_rows[0]['local_ceiling_bar_abs'] != tissue.SURFACE:
        raise AssertionError('Zero tissue surface clamp failed')
    return {'status':'pass','scope':'host local ceiling only; no ascent authorization, schedule, target or real-dive validation',
            'absolute_tolerance_bar':ABS_TOL,'relative_tolerance':0,'comparison_count':len(comparisons),'state_count':len(state_origins),
            'raw_compartment_comparison_count':16*len(comparisons),'repeat_byte_identity':True,
            'compiled_identity':identity,'reference_identity':{k:oracle[k] for k in ('archive_sha256','upstream_model_sha256')},
            'comparisons':comparisons,'gf_policy_cases':gf_cases,'violation_boundary_cases':boundaries,
            'invalid_after_valid_cases':invalid_evidence,'zero_tissues_analytic':{'actual':zero_rows[0],'errors':zero_errors},
            'policy':{'id':tissue.POLICY_ID,'gf_low':0.30,'gf_high':0.70,'pressure_unit':'bar absolute','violation_tolerance_bar':1e-9},
            'independence':'Authored from public header/CLI/spec and pinned upstream. Production numerical sources only fingerprinted as opaque bytes.'}


def evidence_inputs(cli,tissue_cli,archive,build):
    return tissue.evidence_inputs(tissue_cli,archive,build)+[HERE/'ceiling_reference.py',HERE/'test_ceiling.py',cli,HERE/'NOTICE.txt']


def prepare_evidence(cli,tissue_cli,archive,build,report):
    files=evidence_inputs(cli,tissue_cli,archive,build)
    if cli.resolve() != (build/'ceiling_cli').resolve() or tissue_cli.resolve() != (build/'tissue_segments_cli').resolve():
        raise ValueError('CLIs must come from supplied build directory')
    compiler=tissue.configured_compiler(build)
    for path in files+[compiler]:
        if report.resolve()==path.resolve() or (report.exists() and path.exists() and report.samefile(path)):
            raise ValueError('report must not overwrite or alias any evidence input')
    return files,compiler


class CeilingTests(unittest.TestCase):
    def test_full_acceptance(self):
        names=('DECO_CEILING_CLI','DECO_TISSUE_CLI','DECO_REFERENCE_ARCHIVE')
        if any(not os.environ.get(k) for k in names):
            self.fail('Real acceptance inputs required: '+', '.join(names))
        self.assertEqual(exercise(*(os.environ[k] for k in names))['status'],'pass')

    def test_failed_rerun_replaces_previous_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            report=root/'report.json'
            report.write_text('{"status":"pass"}')
            arguments=['--cli',str(root/'ceiling_cli'),'--tissue-cli',str(root/'tissue_cli'),
                       '--archive',str(root/'archive'),'--build-dir',str(root),'--report',str(report)]
            with patch(__name__+'.prepare_evidence',return_value=([],root/'compiler')), \
                 patch(__name__+'.exercise',side_effect=AssertionError('deliberate mismatch')), \
                 patch('builtins.print'):
                self.assertEqual(main(arguments),1)
            failed=json.loads(report.read_text())
            self.assertEqual(failed['status'],'error')
            self.assertIn('deliberate mismatch',failed['error']['message'])

    def test_nonfinite_and_missing_compartments(self):
        for actual in ([1.0], [math.nan]*16, [None]*16):
            with self.assertRaises(AssertionError): compare(actual,[1.0]*16)
        for request in ([],[{'tissues_bar':[1.0]*15,'gf':0.7}],[{'tissues_bar':[0.0]*16,'gf':0.7}]):
            with self.assertRaises(ValueError):ceiling_reference.validate(request)

    def test_archive_verified_before_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            archive=Path(temp)/'bad';archive.write_bytes(b'bad')
            request=[{'tissues_bar':[1.0]*16,'gf':0.7}]
            with patch.object(ceiling_reference.subprocess,'run') as run:
                with self.assertRaises(ceiling_reference.ReferenceError):ceiling_reference.reference(archive,request)
                run.assert_not_called()
            with patch.object(ceiling_reference.tarfile,'open') as extract:
                with self.assertRaises(ceiling_reference.ReferenceError):ceiling_reference.worker(archive,request)
                extract.assert_not_called()

    def test_reference_result_count_guard(self):
        with patch.object(ceiling_reference,'verify'),patch.object(ceiling_reference.subprocess,'run') as run:
            run.return_value.stdout='{"results":[]}'
            with self.assertRaisesRegex(AssertionError,'every requested'):
                ceiling_reference.reference(Path('unused'),[{'tissues_bar':[1.0]*16,'gf':0.7}])

    def test_report_path_and_build_guards(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);build=root/'build';build.mkdir()
            compiler=root/'compiler';compiler.write_text('compiler')
            (build/'CMakeCache.txt').write_text(f'CMAKE_C_COMPILER:FILEPATH={compiler}\n')
            files=[build/'ceiling_cli',build/'tissue_segments_cli',root/'archive',root/'source']
            for path in files:path.write_text('evidence')
            with patch(__name__+'.evidence_inputs',return_value=files):
                for protected in files+[compiler]:
                    for mode in ('direct','symlink','hardlink'):
                        report=protected if mode=='direct' else root/'report'
                        if mode=='symlink':report.symlink_to(protected)
                        if mode=='hardlink':report.hardlink_to(protected)
                        with self.assertRaisesRegex(ValueError,'evidence input'):
                            prepare_evidence(files[0],files[1],files[2],build,report)
                        if mode!='direct':report.unlink()
                with self.assertRaisesRegex(ValueError,'supplied build'):
                    prepare_evidence(root/'other_cli',files[1],files[2],build,root/'out')


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('cli','tissue-cli','archive','build-dir','report'):
        parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args(argv)
    try:
        files,compiler=prepare_evidence(args.cli,args.tissue_cli,args.archive,args.build_dir,args.report)
    except ValueError as exc:
        parser.error(str(exc))
    # Path protection is settled before any report mutation. A failed rerun
    # replaces an old PASS with explicit failure evidence and a nonzero exit.
    try:
        report=exercise(args.cli,args.tissue_cli,args.archive)
        report['file_sha256']={str(p.resolve()):tissue.digest(p) for p in files}
        report['environment']={'python':sys.version,'platform':platform.platform(),'compiler_path':str(compiler.resolve()),
            'compiler':subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0],
            'compile_commands':json.loads((args.build_dir/'compile_commands.json').read_text()),
            'git_head':subprocess.check_output(['git','-C',str(tissue.REPO),'rev-parse','HEAD'],text=True).strip()}
        report['command']=[sys.executable, str(Path(__file__).resolve())]+(list(argv) if argv is not None else sys.argv[1:])
        report['policy_sha256']=__import__('hashlib').sha256(json.dumps(report['policy'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
    except Exception as exc:
        report={'status':'error','error':{'type':type(exc).__name__,'message':str(exc)}}
        args.report.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
        print(json.dumps(report))
        return 1
    args.report.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
    print(json.dumps({k:report[k] for k in ('status','state_count','comparison_count','raw_compartment_comparison_count')}))
    return 0


if __name__=='__main__':
    sys.exit(main())
