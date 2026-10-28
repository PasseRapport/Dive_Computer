# SPDX-License-Identifier: GPL-3.0-or-later
"""One-off ticket09 evidence collector. Run after build_target.py in /tmp/ticket09/target."""
from pathlib import Path
import collections
import hashlib
import io
import json
import re
import shutil
import subprocess
import tarfile

ROOT = Path('/home/Bilal/Documents/GitHub/Dive_Computer')
BASE = ROOT / 'V2/Firmware/ostc4_baseline'
SDK = Path('/home/Bilal/ncs/v3.4.1')
TOOL = Path('/home/Bilal/ncs/toolchains/8285d8ad56/opt/zephyr-sdk/gnu/arm-zephyr-eabi')
BUILD = Path('/tmp/ticket09/target')
OUT = Path('/tmp/ticket09/evidence')
OUT.mkdir(exist_ok=True)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2) + '\n')

def record(path):
    path = Path(path)
    if not path.is_file():
        return {'path': str(path), 'exists': False, 'review': 'unresolved missing dependency'}
    data = path.read_bytes()
    text = data.decode('utf-8', errors='replace')
    ids = [s.strip().rstrip(' */') for s in re.findall(r'SPDX-License-Identifier:\s*([^\r\n*]+)', text)]
    full_gpl = 'either version 3 of the License' in text and 'any later version' in text
    notice = '\n'.join(text.splitlines()[:80])
    return {'path': str(path), 'sha256': sha(data), 'bytes': len(data),
            'spdx_identifiers_as_found': ids, 'explicit_gpl3_or_later_notice': full_gpl,
            'copyright_lines': [s.strip() for s in text.splitlines() if re.search(r'copyright|\(c\)', s, re.I)][:25],
            'notice_excerpt': notice,
            'review': ('file-level SPDX; preserve terms and copyright' if ids else
                       'explicit GPL-3.0-or-later grant' if full_gpl else
                       'manual scope review required; see dossier')}

manifest = json.loads((BASE / 'upstream-manifest.json').read_text())
bundle = Path('/home/Bilal/Documents/apps/Markdown files/spec-decompression-engine/research/ostc4-reuse-evidence-2026-10-27.tar.gz')
assert sha(bundle.read_bytes()) == manifest['audit_bundle_sha256']
retained = []
with tarfile.open(bundle) as audit:
    snapshots = {}
    for name, revision, expected in [
        ('master', '920151ae83c3cc39a6d1f3517ba3d854863e92c3', 'cc60fc4e38677bb7f74c8777aad0eaf47810a7df070d30a94ec77d6f87201221'),
        ('beta', manifest['revision'], manifest['snapshot_archive_sha256'])]:
        data = audit.extractfile(f'ostc4-audit/ostc4-{name}.tar.gz').read()
        assert sha(data) == expected
        with tarfile.open(fileobj=io.BytesIO(data)) as tree:
            prefix = f'ostc4-{revision}/'
            file_hashes = {}
            for rel, expected_file in manifest['files'].items():
                original = tree.extractfile(prefix + rel).read()
                file_hashes[rel] = sha(original)
                if name == 'beta':
                    assert sha(original) == expected_file
                    assert original == (BASE / 'upstream' / rel).read_bytes()
                    item = record(BASE / 'upstream' / rel)
                    item.update(upstream_path=rel, archive_match=True, manifest_match=True,
                                license_conclusion=('GPL-3.0-or-later explicit notice' if item['explicit_gpl3_or_later_notice'] else 'NO FILE-LOCAL GRANT: companion header context only; upstream confirmation required'))
                    retained.append(item)
            settings = tree.extractfile(prefix + 'Discovery/Src/settings.c').read()
            (OUT / f'{name}-settings.txt').write_bytes(settings)
            mapping = prefix + 'base_upstream_revision.txt'
            if mapping in tree.getnames():
                (OUT / f'{name}-base-upstream-revision.txt').write_bytes(tree.extractfile(mapping).read())
            snapshots[name] = {'git_revision': revision, 'archive_sha256': expected,
                               'selected_file_sha256': file_hashes,
                               'settings_sha256': sha(settings),
                               'stable_manufacturer_correspondence_verified': False}
    gpl = audit.extractfile('ostc4-audit/gpl-3.0.html').read()
    assert gpl == (BASE / 'COPYING.html').read_bytes()
save('source-reconciliation.json', {'audit_bundle_sha256': sha(bundle.read_bytes()), 'snapshots': snapshots,
    'selected_revision': manifest['revision'], 'retained_files': retained,
    'different_between_candidates': [k for k in manifest['files'] if snapshots['master']['selected_file_sha256'][k] != snapshots['beta']['selected_file_sha256'][k]],
    'manufacturer_stable_gate': 'OPEN', 'mercurial_mapping_verified': False})

commands = json.loads((BUILD / 'compile_commands.json').read_text())
compiled = sorted(set(c['file'] for c in commands))
deps_text = subprocess.check_output(['ninja', '-C', str(BUILD), '-t', 'deps'], text=True)
(OUT / 'target-dependencies.txt').write_text(deps_text)
deps = sorted(set(str(Path(line.strip()) if Path(line.strip()).is_absolute() else BUILD / line.strip()) for line in deps_text.splitlines() if line.startswith('    ')))
# Ninja's output can include an indented continuation for multi-line dependencies.
# Missing entries are recorded explicitly rather than silently filtered.
save('compiled-source-audit.json', [record(p) for p in compiled])
save('dependency-audit.json', [record(p) for p in deps])

# Audit all tracked baseline source/build/support inputs (exclude prior generated evidence).
tracked = subprocess.check_output(['git', '-C', str(ROOT), 'ls-files', 'V2/Firmware/ostc4_baseline', 'V2/Firmware/src',
    'V2/Firmware/tools', 'V2/Firmware/boards', 'V2/Firmware/CMakeLists.txt', 'V2/Firmware/prj.conf'], text=True).splitlines()
inputs = [p for p in tracked if '/evidence/' not in p and '/fixtures/' not in p]
save('retained-support-audit.json', [record(ROOT / p) for p in inputs])

projects = []
for rel in ['nrf', 'zephyr', 'modules/hal/nordic', 'modules/hal/cmsis_6', 'modules/debug/segger', 'nrfxlib']:
    p = SDK / rel
    projects.append({'path': str(p), 'revision': subprocess.check_output(['git', '-C', str(p), 'rev-parse', 'HEAD'], text=True).strip(),
                     'tracked_modifications': subprocess.check_output(['git', '-C', str(p), 'status', '--porcelain', '--untracked-files=no'], text=True).splitlines()})
save('sdk-projects.json', projects)

licenses = {'zephyr-APACHE-2.0.txt': SDK / 'zephyr/LICENSE', 'cmsis-APACHE-2.0.txt': SDK / 'modules/hal/cmsis_6/LICENSE',
    'nordic-5-clause.txt': SDK / 'nrf/LICENSE', 'picolibc-COPYING.txt': TOOL / 'share/licenses/picolibc/COPYING.picolibc',
    'picolibc-NEWLIB.txt': TOOL / 'share/licenses/picolibc/COPYING.NEWLIB',
    'gcc-RUNTIME-EXCEPTION.txt': TOOL / 'share/licenses/gcc/COPYING.RUNTIME', 'gcc-GPL3.txt': TOOL / 'share/licenses/gcc/COPYING3'}
(OUT / 'licenses').mkdir(exist_ok=True)
for name, p in licenses.items():
    shutil.copyfile(p, OUT / 'licenses' / name)
# These files carry the full non-SPDX permissive grant or the exact problematic grant.
for name, p in {'segger-rtt-notice.txt': SDK / 'modules/debug/segger/SEGGER/SEGGER_RTT.c',
                'nrfx-system-notice.txt': SDK / 'modules/hal/nordic/nrfx/bsp/stable/mdk/system_nrf52833.c',
                'ncs-banner-source.txt': SDK / 'nrf/lib/boot_banner/banner.c'}.items():
    shutil.copyfile(p, OUT / 'licenses' / name)
for src, name in [(BUILD / 'zephyr/zephyr.map', 'zephyr.map'), (BUILD / 'zephyr/.config', 'zephyr.config'),
                   (BUILD / 'compile_commands.json', 'compile_commands.json'), (Path('/tmp/ticket09/target-build.log'), 'target-build.log')]:
    shutil.copyfile(src, OUT / name)
elf = BUILD / 'zephyr/zephyr.elf'
save('audit-summary.json', {'inspected_repository_head': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
    'compiled_entries': len(commands), 'unique_compiled_sources': len(compiled),
    'dependency_entries': len(deps), 'dependency_missing': [p for p in deps if not Path(p).is_file()],
    'support_inputs': len(inputs), 'elf_sha256': sha(elf.read_bytes()),
    'target_execution': False, 'release_approved': False,
    'restrictions': ['Manufacturer stable correspondence unverified', 'Nordic-5-Clause boot banner linked',
                     'buehlmann.c has no file-local grant', 'Generated modification dates missing',
                     'Toolchain runtime exact member license mapping pending', 'No installable release or installation information']})
print((OUT / 'audit-summary.json').read_text())
