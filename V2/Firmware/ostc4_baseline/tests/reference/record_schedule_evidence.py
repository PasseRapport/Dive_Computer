# SPDX-License-Identifier: GPL-3.0-or-later
"""Fresh-build, source-bound evidence recorder; strict numerical exit by default."""
import argparse
import json
from pathlib import Path
import subprocess

from compare_schedule import evaluate, sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build',type=Path)
    parser.add_argument('output',type=Path)
    parser.add_argument('--record',action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build = args.build.resolve()
    cache = {}
    for line in (build/'CMakeCache.txt').read_text().splitlines():
        if line and not line.startswith(('#','//')) and '=' in line and ':' in line:
            key,value = line.split('=',1)
            cache[key.split(':',1)[0]] = value
    if Path(cache['CMAKE_HOME_DIRECTORY']).resolve()!=root:
        raise SystemExit('build source directory does not match this reference tree')
    manifest = json.loads((root/'upstream-manifest.json').read_text())
    for name,digest in manifest['files'].items():
        if sha(root/'upstream'/name)!=digest:
            raise SystemExit('upstream source identity changed: '+name)
    subprocess.run(['cmake','--build',str(build),'--clean-first','--target','ostc4_continuity_replay'],check=True)
    report = evaluate(build/'ostc4_continuity_replay',args.output)
    files = sorted([*root.glob('src/*.[ch]'),*root.glob('include/*.h'),*root.glob('compat/*.h'),
                    *root.glob('corrections/*'),root/'CMakeLists.txt'])
    links = {str(p.relative_to(build)):p.read_text() for p in build.glob('CMakeFiles/*/link.txt')}
    if not links:
        links['ninja_commands'] = subprocess.check_output([cache['CMAKE_MAKE_PROGRAM'],'-C',str(build),'-t','commands','ostc4_continuity_replay'],text=True)
    bundle = dict(upstream=manifest,source_sha256={str(p.relative_to(root)):sha(p) for p in files if p.is_file()},
        generated_sha256={str(p.relative_to(build)):sha(p) for p in (build/'corrected').glob('*.c')},
        compile_commands=json.loads((build/'compile_commands.json').read_text()),link_commands=links,
        reference_files_sha256={p.name:sha(p) for p in Path(__file__).resolve().parent.glob('*') if p.is_file()},
        comparison_report_sha256=sha(args.output/'schedule-comparison.json'),accepted=report['accepted'],
        output_sha256={p.name:sha(p) for p in args.output.glob('*.jsonl')},
        evidence_level='synthetic PC only; no target compilation or physical execution')
    (args.output/'build-provenance.json').write_text(json.dumps(bundle,indent=2)+'\n')
    print(json.dumps(dict(accepted=report['accepted'],output=str(args.output)),indent=2))
    raise SystemExit(0 if report['accepted'] or args.record else 1)

if __name__=='__main__':
    main()
