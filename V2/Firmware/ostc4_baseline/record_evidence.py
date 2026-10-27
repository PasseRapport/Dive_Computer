# SPDX-License-Identifier: GPL-3.0-or-later
"""Record synthetic observations and the actual build inputs. No schedule oracle."""
import argparse
import hashlib
import json
import pathlib
import platform
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('build', type=pathlib.Path)
parser.add_argument('output', type=pathlib.Path)
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parent
build, output = args.build.resolve(), args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
# Bind evidence to this source tree and force a fresh build, so an old or
# unrelated executable cannot be attributed to the selected source manifest.
cache = {}
for line in (build / 'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#', '//')) and '=' in line and ':' in line:
        key, value = line.split('=', 1)
        cache[key.split(':', 1)[0]] = value
if pathlib.Path(cache['CMAKE_HOME_DIRECTORY']).resolve() != root:
    raise SystemExit('build directory is configured for another source tree')
subprocess.run(['cmake', '--build', str(build), '--target', 'ostc4_replay',
                '--clean-first'], check=True)
cli = build / 'ostc4_replay'
digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
manifest = json.loads((root / 'upstream-manifest.json').read_text())
for name, expected in manifest['files'].items():
    if digest(root / 'upstream' / name) != expected:
        raise SystemExit('changed upstream file: ' + name)
policy = json.loads((root / 'policy.json').read_text())
records = []
for name in ('shallow', 'decompression'):
    profile = root / 'fixtures' / (name + '.csv')
    replay = subprocess.check_output([str(cli), str(profile)])
    if replay != subprocess.check_output([str(cli), str(profile)]):
        raise SystemExit('nonrepeatable output: ' + name)
    # Ensure malformed JSON/nonfinite JSON values cannot enter an evidence bundle.
    for row in replay.splitlines():
        json.loads(row, parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
    metadata = json.loads(replay.splitlines()[0])
    if (metadata['revision'] != manifest['revision'] or
            metadata['policy'] != policy['identity'] or
            metadata['correction_set'] != manifest['correction_set']):
        raise SystemExit('compiled source/policy identity mismatch')
    (output / (name + '.jsonl')).write_bytes(replay)
    records.append({'profile': profile.name, 'input_sha256': digest(profile),
                    'input_csv': profile.read_text(),
                    'output_sha256': hashlib.sha256(replay).hexdigest()})
commands = json.loads((build / 'compile_commands.json').read_text())
links = {str(path.relative_to(build)): path.read_text()
         for path in build.glob('CMakeFiles/*/link.txt')}
if not links:
    if cache['CMAKE_GENERATOR'] not in ('Ninja', 'Ninja Multi-Config'):
        raise SystemExit('cannot capture linker commands for this CMake generator')
    links['ninja_commands'] = subprocess.check_output(
        [cache['CMAKE_MAKE_PROGRAM'], '-C', str(build), '-t', 'commands', 'ostc4_replay'], text=True)
report = {'kind' : 'synthetic baseline observations, not independent expected schedules',
          'source': manifest, 'policy': policy,
          'host_math_runtime': {'platform': platform.platform(), 'libc': platform.libc_ver()},
          'adapter_files_sha256': {str(p.relative_to(root)): digest(p) for p in
              sorted([*root.glob('src/*.c'), *root.glob('include/*.h'), *root.glob('compat/*.h')])},
          'executable_sha256': digest(cli), 'compile_commands': commands,
          'link_commands': links, 'profiles': records,
          'pc': 'headless build and replay', 'target_build': 'not performed for this ticket',
          'physical': 'not performed', 'numerical_acceptance': 'not established'}
(output / 'reproduction.json').write_text(json.dumps(report, indent=2) + '\n')
print(output)
