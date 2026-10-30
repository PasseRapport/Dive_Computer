# SPDX-License-Identifier: GPL-3.0-or-later
"""Compare JSONL public bench observations to pre-frozen integer expectations.
Usage: python3 compare_public.py BENCH_RUNNER --field desaturation_minutes --output report.json
Runner must accept existing gf_bench_replay commands on stdin, emitting one JSON
observation after fresh/advance; finish/arm may emit no observation. Failures stay
visible in the JSON. Never regenerates expectations from observed tissues/results.
"""
import argparse,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('runner',type=pathlib.Path);p.add_argument('--field',default='desaturation_minutes');p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).parent
fixture=root/'expected.json'; expected=json.loads(fixture.read_text())
report={'expected_sha256':hashlib.sha256(fixture.read_bytes()).hexdigest(),'runner_sha256':hashlib.sha256(a.runner.read_bytes()).hexdigest(),'field':a.field,'profiles':{},'pass':True,'scope':'PC public bench execution only'}
for name,item in expected['profiles'].items():
    proc=subprocess.run([str(a.runner)],input=(root/(name+'.commands.txt')).read_text(),text=True,capture_output=True)
    observations=[json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    by_time={x['timestamp_seconds']:x for x in observations}
    points=[]
    for want in item['selected_checkpoints']:
        t=want['timestamp_seconds']; actual=by_time.get(t,{}).get(a.field)
        target=want['expected']['minutes']
        points.append({'timestamp_seconds':t,'expected_minutes':target,'actual_minutes':actual,'difference_minutes':None if actual is None else actual-target,'pass':actual==target})
    ok=proc.returncode==0 and all(x['pass'] for x in points)
    report['profiles'][name]={'pass':ok,'exit_code':proc.returncode,'stderr':proc.stderr,'observations':len(observations),'points':points,'raw_jsonl':proc.stdout}
    report['pass']=report['pass'] and ok
    print(name, 'PASS' if ok else 'FAIL',[(x['timestamp_seconds'],x['difference_minutes']) for x in points])
a.output.write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if report['pass'] else 1)
