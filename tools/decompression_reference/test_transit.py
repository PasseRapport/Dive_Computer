#!/usr/bin/env python3
"""Independent transit comparisons with mandatory real evidence and failure reports."""
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

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import transit_reference as oracle
from runner import ReferenceError
import test_tissue_integration as tissue
from test_ceiling import compiled


def case(name, **kwargs):
    c=dict(name=name,mode='fixed',surface=1.,start=2.,dest=1.,gf=.7,
           allowance=1000,identity=0,tissues_bar=[.79*(1.-.0627)]*16)
    c.update(kwargs)
    return c


def command(c):
    policy=c['gf'] if c['mode']=='fixed' else c.get('anchor','none')
    if policy is None:policy='none'
    return ' '.join(map(str,[c['mode'],repr(c['surface']),repr(c['start']),repr(c['dest']),policy,
                            c['allowance'],c['identity']]+[repr(x) for x in c['tissues_bar']]))


def fixtures(archive,tissue_cli):
    cases=[];origins=[]
    table=json.loads((HERE/'nitrogen_coefficients.v1.json').read_text())
    # Every Story 1.3 captured state from both origins, without truncation.
    for name,segments in tissue.FIXTURES.items():
        ref=tissue.reference(archive,tissue.SURFACE,segments)
        states=tissue.reference_states(ref,segments)
        constants,cstates,_=tissue.compiled(tissue_cli,tissue.SURFACE,segments)
        tissue.table_gate(constants,table,ref['coefficients'])
        if len(cstates)!=len(states):raise AssertionError('Captured state count mismatch')
        for origin,values in [('C_measured',cstates),('isolated_reference',states)]:
            for i,state in enumerate(values):
                origins.append(dict(fixture=name,origin=origin,step=i,tissues_bar=state))
                for mode,anchor in [('fixed',None),('line',2.),('line',None)]:
                    cases.append(case(f'{name}_{origin}_{i}_{mode}_{anchor}',mode=mode,anchor=anchor,
                                      surface=tissue.SURFACE,start=4.01325,dest=tissue.SURFACE,tissues_bar=state))
    expected_origins=2*sum(len(s)+1 for s in tissue.FIXTURES.values())
    if expected_origins!=64 or len(origins)!=64 or len(cases)!=192:
        raise AssertionError('Exact captured comparison coverage mismatch')
    r=oracle.RATE
    cases += [case('zero_pass',start=1.,dest=1.),case('zero_reject',start=1.,dest=1.,tissues_bar=[3.]*16),
              case('zero_nitrogen',start=1.,dest=1.,tissues_bar=[0.]*16),
              case('start_reject',tissues_bar=[4.]*16),case('interior_reject',tissues_bar=[1.7]*16),
              case('fractional_pass',start=1.1,dest=1.1-r*1.25),
              case('exact_integer_grid',surface=.8,start=r*128,dest=r*64),
              case('nextbelow_integer',surface=.8,start=math.nextafter(r*128,-math.inf),dest=r*64),
              case('nextabove_integer',surface=.8,start=math.nextafter(r*128,math.inf),dest=r*64),
              case('line_cross_anchor',mode='line',anchor=1.5),
              case('line_absent',mode='line',anchor=None),
              case('interior_only_reject',mode='line',surface=1.,start=3.646353675578058,dest=1.,
                   anchor=1.057828764880202,tissues_bar=[.1]*4+[1.4753273211907367]+[.1]*11)]
    # Construct a first violation at the exact fractional destination, using the
    # linearity of the independently defined Schreiner uptake in captured pN2.
    end=1.1-r*1.25;duration=(1.1-end)/r;gf=.7;k=15
    a=float(table['rows'][k]['a']);b=float(table['rows'][k]['b']);half=float(table['rows'][k]['half_life'])
    rate=-r*.79*60;kconst=math.log(2)/half;minutes=duration/60
    inspired=.79*(1.1-.0627);decay=math.exp(-kconst*minutes)
    base=inspired+rate*(minutes-1/kconst)-(inspired-rate/kconst)*decay
    target=(end+1e-4)*(1-gf+gf/b)+gf*a
    pressures=[.1]*16;pressures[k]=(target-base)/decay
    cases.append(case('endpoint_reject',start=1.1,dest=end,tissues_bar=pressures))
    base_cases=list(cases[-13:])
    for c in base_cases:
        duration=(c['start']-c['dest'])/r;required=math.ceil(duration)+1
        for allowance in (0,required-1,required):
            cases.append(dict(c,name=c['name']+'_allowance_'+str(allowance),allowance=allowance))
    for surface in (1., .0627, .1):
        cases.append(case('tiny_zero_nitrogen_'+repr(surface),surface=surface,
                          start=math.nextafter(surface,math.inf),dest=surface,tissues_bar=[0.]*16,allowance=3))
    cases.append(case('huge_permitted_start_reject',start=1e10,tissues_bar=[1e11]*16,allowance=2**64-1))
    # Invalid scenarios are deliberately interleaved after PASS in exercise.
    invalid=[]
    for key,values in [('gf',[0.,-.1,1.01,math.nan,math.inf]),('surface',[0.,math.nan,math.inf]),
                       ('start',[.9,math.nan,math.inf]),('dest',[.9,2.1,math.nan,math.inf])]:
        for v in values:invalid.append(case('invalid_'+key+'_'+repr(v),**{key:v}))
    for anchor in (1.,.9,math.nan,math.inf):invalid.append(case('invalid_anchor_'+repr(anchor),mode='line',anchor=anchor))
    for identity in (1,2):invalid.append(case('invalid_identity_'+str(identity),identity=identity))
    for i in range(16):
        for v in (-.1,math.nan,math.inf):
            values=[1.]*16;values[i]=v
            invalid.append(case(f'invalid_tissue_{i}_{v}',tissues_bar=values))
    invalid += [case('numeric_duration_overflow',start=sys.float_info.max,allowance=2**64-1),
                case('huge_grid_limit',start=1e20,allowance=2**64-1)]
    for c in invalid:cases.extend([case('pass_before_'+c['name'],start=1.,dest=1.),c])
    return cases,origins,constants


def compare(c,a,e):
    # Status equality is unconditional: tolerance NEVER rescues disagreement.
    if a['status']!=e['status']:raise AssertionError(f"{c['name']}: verdict {a['status']} != {e['status']}")
    for key in ('inspected_points','destination_valid','diagnostic_valid'):
        if a[key]!=e[key]:raise AssertionError(f"{c['name']}: {key} differs")
    if not a['input_unchanged'] or not a['anchor_unchanged']:raise AssertionError('Captured inputs changed')
    errors={}
    if 'arrival_seconds' in e:
        av=a['arrival_seconds'];ev=e['arrival_seconds']
        if ev is not None:
            if av!=ev:raise AssertionError('Binary64 arrival differs')
        elif av is not None:raise AssertionError('Overflow duration must be unavailable')
    if e['status']==0:
        errors['tissues']=tissue.compare(a['destination_nitrogen_bar'],e['destination_nitrogen_bar'])
        tissue.require_pass(errors['tissues'])
    elif a['destination_nitrogen_bar'] is not None:raise AssertionError('Invalid completion exposes tissues')
    fields=('seconds','pressure_bar_abs','gf','ceiling_bar_abs','margin_bar')
    for field in fields:
        key='failure_'+field
        if e['status']==1:
            av,ev=a[key],e[key];err=abs(av-ev) if av is not None else math.inf
            limit=1e-6 if field in ('ceiling_bar_abs','margin_bar') else (2e-14 if field=='gf' else 0)
            if not math.isfinite(err) or err>limit:raise AssertionError(f"{c['name']}: {key} mismatch {av} {ev}")
            errors[key]=err
        elif a[key] is not None:raise AssertionError('Invalid rejection diagnostics exposed')
    return errors


class ComparisonFailure(AssertionError):
    def __init__(self, message, context):
        super().__init__(message)
        self.context = context


def require_identity(identity, constants):
    keys=('policy_id','coefficient_id','coefficient_sha256')
    if identity != {key:constants[key] for key in keys}:
        raise AssertionError('CLI identities must contain complete expected policy/coefficient/hash values')


def compare_all(cases, commands, actual, expected):
    completed=[]
    if not (len(cases)==len(commands)==len(actual)==len(expected)):
        raise ComparisonFailure('Comparison response counts differ',dict(inputs=cases,raw_commands=commands,actual=actual,reference=expected,completed_comparisons=[]))
    for c,cmd,a,e in zip(cases,commands,actual,expected):
        row={'input':c,'raw_command':cmd,'actual':a,'reference':e}
        try:
            row['errors']=compare(c,a,e)
        except Exception as exc:
            raise ComparisonFailure(str(exc),dict(failing_comparison=row,completed_comparisons=completed)) from exc
        completed.append(row)
    return completed


def exercise(cli,tissue_cli,archive):
    cases,origins,constants=fixtures(archive,tissue_cli)
    expected=oracle.reference(archive,cases)
    tissue.table_gate(constants,json.loads((HERE/'nitrogen_coefficients.v1.json').read_text()),expected['coefficients'])
    # Probe each independent interior grid point as an exact destination of a
    # separate request. This observes production prefix integration and count,
    # while the oracle still derives every prefix duration independently.
    original=list(zip(cases,expected['results']))
    probes=[]
    for c,e in original:
        if c['name'] in ('exact_integer_grid','fractional_pass','line_cross_anchor'):
            probes.extend(dict(c,name=c['name']+'_prefix_'+str(i),dest=point['pressure_bar_abs'])
                          for i,point in enumerate(e['grid']))
    probe_oracle=oracle.reference(archive,probes)
    cases.extend(probes);expected['results'].extend(probe_oracle['results'])
    commands=[command(c) for c in cases]
    identity,actual,raw=compiled(cli,commands)
    require_identity(identity,constants)
    if compiled(cli,commands)[2]!=raw:raise AssertionError('Repeated transit output differs')
    comparisons=compare_all(cases,commands,actual,expected['results'])
    byname={c['name']:(c,a,e) for c,a,e in zip(cases,actual,expected['results'])}
    for name in ('start_reject','interior_reject','endpoint_reject','interior_only_reject','zero_reject'):
        c,a,e=byname[name]
        if e['status']!=1:raise AssertionError(name+' must actually reject')
        t=e['failure_seconds'];arrival=e['arrival_seconds']
        if name=='endpoint_reject' and t!=arrival:raise AssertionError('Endpoint fixture failed before destination')
        if name in ('interior_reject','interior_only_reject') and not 0<t<arrival:raise AssertionError('Expected interior first rejection')
        if name in ('start_reject','zero_reject') and t!=0:raise AssertionError('Expected start rejection')
    for name in ('zero_pass','zero_reject','zero_nitrogen'):
        if byname[name][2]['inspected_points']!=1:raise AssertionError('Equal endpoints require exactly one check')
    integer=byname['exact_integer_grid'][2]
    if integer['arrival_seconds']!=64. or [p['seconds'] for p in integer['grid']]!=list(range(65)):
        raise AssertionError('Exact integer arrival grid differs')
    line=byname['line_cross_anchor'][2]['grid']
    if not any(p['pressure_bar_abs']>1.5 and p['gf']==.30 for p in line) or not any(1.<p['pressure_bar_abs']<1.5 and .30<p['gf']<.70 for p in line) or line[-1]['gf']!=.70:
        raise AssertionError('Retained line must cross anchor and interpolate to surface')
    if any(p['gf']!=.70 for p in byname['line_absent'][2]['grid']):raise AssertionError('Absent anchor must use GF high')
    for c,a,e in byname.values():
        if '_allowance_' in c['name']:
            required=math.ceil((c['start']-c['dest'])/oracle.RATE)+1
            if c['allowance']<required and (e['status']!=5 or e['inspected_points']!=0):raise AssertionError('Insufficient allowance must preflight LIMIT with no checks')
    # Confirm interior-only failure has a safe endpoint, integrating the full
    # direct segment independently using the same oracle with permissive GF=1.
    c,_,e=byname['interior_only_reject']
    endpoint=oracle.reference(archive,[dict(c,mode='fixed',gf=1.)])['results'][0]
    if endpoint['status']!=0:raise AssertionError('Endpoint control must complete')
    final=oracle.reference(archive,[dict(c,start=c['dest'],dest=c['dest'],tissues_bar=endpoint['destination_nitrogen_bar'])])['results'][0]
    if final['status']!=0:raise AssertionError('Interior-only fixture destination must satisfy GF line')
    for c,_,e in byname.values():
        grid=e['grid']
        if grid:
            if grid[0]['seconds']!=0:raise AssertionError('Grid must start at zero')
            if any(b['seconds']<=a['seconds'] for a,b in zip(grid,grid[1:])):raise AssertionError('Grid points repeated/out of order')
            if e['status']==0 and (grid[-1]['seconds']!=e['arrival_seconds'] or grid[-1]['pressure_bar_abs']!=c['dest']):raise AssertionError('Missing exact destination')
    successful=[(c,a,e) for c,a,e in zip(cases,actual,expected['results']) if e['status']==0]
    ceiling_commands=[' '.join(map(str,['fixed',c['surface'],e['grid'][-1]['gf'],0]+a['destination_nitrogen_bar'])) for c,a,e in successful]
    ceiling_identity,ceilings,_=compiled(Path(cli).parent/'ceiling_cli',ceiling_commands)
    require_identity(ceiling_identity,constants)
    final_ceilings=[]
    for (c,a,e),ceiling in zip(successful,ceilings):
        expected_ceiling=e['grid'][-1]['ceiling_bar_abs']
        err=abs(ceiling['local_ceiling_bar_abs']-expected_ceiling)
        if ceiling['status']!=0 or not math.isfinite(err) or err>1e-6:raise AssertionError('Destination ceiling mismatch')
        final_ceilings.append(dict(name=c['name'],actual_bar=ceiling['local_ceiling_bar_abs'],reference_bar=expected_ceiling,absolute_error_bar=err))
    return dict(status='pass',scope='Host discrete virtual ascent only; no continuous-time, schedule, target or hardware validation',
                state_count=len(origins),comparison_count=len(cases),prefix_probe_count=len(probes),destination_ceiling_comparisons=final_ceilings,captured_state_origins=origins,comparisons=comparisons,
                compiled_identity=identity,reference_identity={k:expected[k] for k in ('archive_sha256','upstream_model_sha256')},
                repeat_byte_identity=True,interior_only_endpoint_control={'integrated':endpoint,'endpoint':final},
                policy=dict(id=tissue.POLICY_ID,rate_bar_per_second=oracle.RATE,density_kg_m3=1025.,gravity_m_s2=9.80665,ascent_m_min=9.,
                    grid='start; integer seconds strictly before binary64 arrival; exact destination once; zero-distance once',
                    allowance='preflight ceil(arrival)+1, zero-distance one; numeric overflow error; >=2**53-1 LIMIT',
                    violation='pressure + 1e-9 < surface-clamped local ceiling',gf_low=.30,gf_high=.70),
                tolerance=dict(tissue_absolute_bar=1e-7,tissue_relative=1e-7,ceiling_absolute_bar=1e-6,verdict='exact'),
                independence='Authored from approved spec, public headers, CLI and optional numeric C fixture input. Production numerical sources only fingerprinted as opaque bytes.')


def evidence_inputs(cli,tissue_cli,archive,build):
    return tissue.evidence_inputs(tissue_cli,archive,build)+[HERE/n for n in ('transit_reference.py','test_transit.py','test_ceiling.py','ceiling_reference.py','NOTICE.txt')]+[cli,build/'ceiling_cli',HERE/'.cache/cmas-table25.webp']


class ProtectedReportError(ValueError):
    """Writing a failure report here would destroy an evidence input."""


def protect_report(files, compiler_paths, report):
    for path in files+compiler_paths:
        if report.resolve()==path.resolve() or (report.exists() and path.exists() and report.samefile(path)):
            raise ProtectedReportError('report must not overwrite or alias any evidence input')


def protect_evidence(cli,tissue_cli,archive,build,report):
    files=evidence_inputs(cli,tissue_cli,archive,build)
    # Protect known paths first, even if cache/build binding is missing/broken.
    protect_report(files,[],report)
    cache=build/'CMakeCache.txt'
    if cache.is_file():
        # Protect configured compiler bytes before validation can fail. Relative
        # paths are invalid, but still protected from destructive reporting.
        compilers=[Path(line.partition('=')[2]) for line in cache.read_text().splitlines()
                   if line.partition('=')[0].startswith('CMAKE_C_COMPILER:') and '=' in line]
        protect_report(files,compilers,report)
    return files


def prepare_evidence(cli,tissue_cli,archive,build,report):
    files=protect_evidence(cli,tissue_cli,archive,build,report)
    for path,name in ((cli,'transit_cli'),(tissue_cli,'tissue_segments_cli')):
        if path.resolve()!=(build/name).resolve():raise ValueError('CLIs must come from supplied build directory')
    compiler=tissue.configured_compiler(build)
    protect_report(files,[compiler],report)
    return files,compiler


def clean(value):
    if isinstance(value,float) and not math.isfinite(value):return repr(value)
    if isinstance(value,list):return [clean(x) for x in value]
    if isinstance(value,dict):return {k:clean(v) for k,v in value.items()}
    return value


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('cli','tissue-cli','archive','build-dir','report'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args(argv)
    reproduction=[sys.executable,str(Path(__file__).resolve())]+(list(argv) if argv is not None else sys.argv[1:])
    try:
        files,compiler=prepare_evidence(args.cli,args.tissue_cli,args.archive,args.build_dir,args.report)
        report=exercise(args.cli,args.tissue_cli,args.archive)
        report['file_sha256']={str(x.resolve()):tissue.digest(x) for x in files+[compiler]}
        report['environment']=dict(python=sys.version,platform=platform.platform(),compiler_path=str(compiler.resolve()),
            compiler=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0],
            compile_commands=json.loads((args.build_dir/'compile_commands.json').read_text()),
            git_head=subprocess.check_output(['git','-C',str(tissue.REPO),'rev-parse','HEAD'],text=True).strip())
        ctest=subprocess.run(['ctest','--test-dir',str(args.build_dir),'--output-on-failure'],text=True,capture_output=True,check=True)
        report['host_ctest_output']=ctest.stdout
        report['command']=[sys.executable,str(Path(__file__).resolve())]+(list(argv) if argv is not None else sys.argv[1:])
        report['policy_sha256']=hashlib.sha256(json.dumps(report['policy'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
    except ProtectedReportError as exc:
        print(json.dumps({'status':'error','error':{'type':type(exc).__name__,'message':str(exc)},'command':reproduction}));return 1
    except Exception as exc:
        report={'status':'error','error':{'type':type(exc).__name__,'message':str(exc)},'command':reproduction}
        if isinstance(exc,ComparisonFailure):report['error'].update(exc.context)
        report=clean(report)
        args.report.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n');print(json.dumps(report,allow_nan=False));return 1
    args.report.write_text(json.dumps(clean(report),indent=2,allow_nan=False)+'\n')
    print(json.dumps({k:report[k] for k in ('status','state_count','comparison_count')}));return 0


class TransitTests(unittest.TestCase):
    def test_full_acceptance(self):
        names=('DECO_TRANSIT_CLI','DECO_TISSUE_CLI','DECO_REFERENCE_ARCHIVE')
        if any(not os.environ.get(n) for n in names):self.fail('Real acceptance inputs required: '+', '.join(names))
        self.assertEqual(exercise(*(os.environ[n] for n in names))['status'],'pass')

    def test_failed_rerun_replaces_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);report=root/'report';report.write_text('{"status":"pass"}')
            args=['--cli',str(root/'transit_cli'),'--tissue-cli',str(root/'tissue_segments_cli'),'--archive',str(root/'archive'),'--build-dir',str(root),'--report',str(report)]
            with patch(__name__+'.prepare_evidence',return_value=([],root/'compiler')),patch(__name__+'.exercise',side_effect=AssertionError('deliberate disagreement')),patch('builtins.print'):
                self.assertEqual(main(args),1)
            self.assertEqual(json.loads(report.read_text())['status'],'error')

    def test_tiny_zero_nitrogen_ascent(self):
        archive=os.environ.get('DECO_REFERENCE_ARCHIVE')
        cli=os.environ.get('DECO_TRANSIT_CLI')
        if not archive or not cli:self.fail('Real tiny-ascent inputs required')
        cases=[case('tiny_'+str(surface),surface=surface,start=math.nextafter(surface,math.inf),
                    dest=surface,tissues_bar=[0.]*16,allowance=3) for surface in (1.,.0627,.1)]
        expected=oracle.reference(archive,cases)['results']
        _,actual,_=compiled(cli,[command(c) for c in cases])
        for c,a,e in zip(cases,actual,expected):
            compare(c,a,e)
            self.assertEqual(e['status'],0)
            self.assertEqual(e['inspected_points'],2)
            self.assertTrue(all(math.isfinite(v) and v>0 for v in e['destination_nitrogen_bar']))
            self.assertTrue(all(math.isfinite(v) and v>=0 for v in a['destination_nitrogen_bar']))
            provenance=e['grid'][-1]['numerical_provenance']
            self.assertEqual(provenance['integration'],'decimal_schreiner')
            self.assertEqual(provenance['decimal_precision'],100)
            self.assertEqual(len(provenance['raw_upstream_tissues_bar']),16)
        self.assertTrue(any(v<0 for e in expected for v in e['grid'][-1]['numerical_provenance']['raw_upstream_tissues_bar']))

    def test_huge_permitted_grid_rejects_without_materializing_offsets(self):
        import inspect
        import itertools
        offsets=oracle.inspection_offsets(1e12)
        self.assertTrue(inspect.isgenerator(offsets))
        self.assertEqual(list(itertools.islice(offsets,3)),[0.,1.,2.])
        archive=os.environ.get('DECO_REFERENCE_ARCHIVE')
        cli=os.environ.get('DECO_TRANSIT_CLI')
        if not archive or not cli:self.fail('Real huge-grid inputs required')
        c=case('huge',start=1e10,tissues_bar=[1e11]*16,allowance=2**64-1)
        e=oracle.reference(archive,[c])['results'][0]
        _,actual,_=compiled(cli,[command(c)])
        compare(c,actual[0],e)
        self.assertEqual(e['status'],1)
        self.assertEqual(e['inspected_points'],1)
        self.assertEqual(len(e['grid']),1)

    def test_complete_identity_required(self):
        constants=dict(policy_id='policy',coefficient_id='coefficient',coefficient_sha256='hash')
        require_identity(constants,constants)
        invalid=[{},dict(constants,extra='unexpected'),dict(constants,policy_id='wrong')]
        invalid += [{k:v for k,v in constants.items() if k!=missing} for missing in constants]
        for identity in invalid:
            with self.assertRaisesRegex(AssertionError,'complete expected'):require_identity(identity,constants)

    def test_safe_preflight_errors_replace_stale_pass_and_preserve_aliases(self):
        for failure in ('missing_cache','wrong_cli'):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as temp:
                root=Path(temp);compiler=root/'compiler';compiler.write_text('compiler')
                cli=root/('other_cli' if failure=='wrong_cli' else 'transit_cli')
                tissue_cli=root/'tissue_segments_cli';archive=root/'archive';source=root/'source'
                cache=root/'CMakeCache.txt'
                if failure=='wrong_cli':cache.write_text(f'CMAKE_C_COMPILER:FILEPATH={compiler}\n')
                inputs=[cli,tissue_cli,archive,source,cache]
                for path in inputs[:-1]:path.write_text('evidence')
                report=root/'report'
                def arguments(path):
                    return ['--cli',str(cli),'--tissue-cli',str(tissue_cli),'--archive',str(archive),'--build-dir',str(root),'--report',str(path)]
                with patch(__name__+'.evidence_inputs',return_value=inputs),patch('builtins.print'):
                    report.write_text('{"status":"pass"}')
                    self.assertEqual(main(arguments(report)),1)
                    self.assertEqual(json.loads(report.read_text())['status'],'error')
                    protected=inputs[:-1]+([compiler,cache] if cache.exists() else [])
                    for path in protected:
                        before=path.read_bytes()
                        for alias in ('direct','symlink','hardlink'):
                            dest=path if alias=='direct' else root/('alias_'+alias)
                            if alias=='symlink':dest.symlink_to(path)
                            if alias=='hardlink':dest.hardlink_to(path)
                            self.assertEqual(main(arguments(dest)),1)
                            self.assertEqual(path.read_bytes(),before)
                            if alias!='direct':dest.unlink()

    def test_disagreement_report_retains_context_and_reproduction(self):
        valid=case('completed',tissues_bar=[math.nan]*16)
        failing=case('mismatch',tissues_bar=[math.inf]*16)
        state_error=dict(status=3,inspected_points=0,destination_valid=False,diagnostic_valid=False,
                         destination_nitrogen_bar=None,input_unchanged=True,anchor_unchanged=True,
                         **{'failure_'+k:None for k in ('seconds','pressure_bar_abs','gf','ceiling_bar_abs','margin_bar')})
        with self.assertRaises(ComparisonFailure) as caught:
            compare_all([valid,failing],[command(valid),command(failing)],[state_error,{'status':0}],[dict(state_error),{'status':1}])
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);report=root/'report'
            args=['--cli',str(root/'transit_cli'),'--tissue-cli',str(root/'tissue_segments_cli'),'--archive',str(root/'archive'),'--build-dir',str(root),'--report',str(report)]
            with patch(__name__+'.prepare_evidence',return_value=([],root/'compiler')),patch(__name__+'.exercise',side_effect=caught.exception),patch('builtins.print'):
                self.assertEqual(main(args),1)
            evidence=json.loads(report.read_text())
            self.assertEqual(evidence['status'],'error')
            self.assertEqual(evidence['command'][2:],args)
            context=evidence['error']
            self.assertEqual(len(context['completed_comparisons']),1)
            self.assertEqual(context['completed_comparisons'][0]['input']['tissues_bar'],['nan']*16)
            failed=context['failing_comparison']
            self.assertEqual(failed['input']['tissues_bar'],['inf']*16)
            self.assertEqual(failed['raw_command'],command(failing))
            self.assertEqual(failed['actual'],{'status':0})
            self.assertEqual(failed['reference'],{'status':1})

    def test_archive_verified_before_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            archive=Path(temp)/'bad';archive.write_bytes(b'bad');cases=[case('valid')]
            with patch.object(oracle.subprocess,'run') as execute:
                with self.assertRaises(ReferenceError):oracle.reference(archive,cases)
                execute.assert_not_called()
            with patch.object(oracle.tarfile,'open') as extract:
                with self.assertRaises(ReferenceError):oracle.worker(archive,cases)
                extract.assert_not_called()

    def test_count_guard(self):
        with patch.object(oracle,'verify'),patch.object(oracle.subprocess,'run') as run:
            run.return_value.stdout='{"results":[]}'
            with self.assertRaisesRegex(AssertionError,'every requested'):oracle.reference('unused',[case('one')])

    def test_invalid_shape(self):
        for cases in ([],[case('short',tissues_bar=[1.]*15)],[case('bad_allowance',allowance=True)]):
            with self.assertRaises(ValueError):oracle.validate(cases)

    def test_verdict_disagreement_is_never_tolerated(self):
        with self.assertRaisesRegex(AssertionError,'verdict'):compare(case('negative_control'),{'status':0},{'status':1})

    def test_evidence_path_guards(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);compiler=root/'compiler';compiler.write_text('compiler')
            files=[root/n for n in ('transit_cli','tissue_segments_cli','archive','source')]
            for x in files:x.write_text('evidence')
            (root/'CMakeCache.txt').write_text(f'CMAKE_C_COMPILER:FILEPATH={compiler}\n')
            with patch(__name__+'.evidence_inputs',return_value=files):
                for protected in files+[compiler]:
                    for mode in ('direct','symlink','hardlink'):
                        report=protected if mode=='direct' else root/'report'
                        if mode=='symlink':report.symlink_to(protected)
                        if mode=='hardlink':report.hardlink_to(protected)
                        with self.assertRaisesRegex(ValueError,'evidence input'):prepare_evidence(files[0],files[1],files[2],root,report)
                        if mode!='direct':report.unlink()
                with self.assertRaisesRegex(ValueError,'supplied build'):prepare_evidence(root/'other',files[1],files[2],root,root/'report')

if __name__=='__main__':sys.exit(main())
