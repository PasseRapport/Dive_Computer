"""Separate native processes exercise durable preferences through public UI inputs."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
binary=Path(sys.argv[1]).resolve()
evidence=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else None
if evidence:evidence.mkdir(parents=True,exist_ok=True)
def run(directory,mode,choice):
    label=f'choice-{choice}' if directory.name.startswith('apnea-restart-') else directory.name
    gallery=(evidence/f'{label}-{mode}') if evidence else directory/f'capture-{mode}'
    gallery.mkdir(exist_ok=True)
    env=os.environ.copy()
    for key in list(env):
        if key.startswith('DIVE_SIM_'):del env[key]
    env.update(SDL_VIDEODRIVER='dummy',DIVE_WATCH_STATE_DIR=str(directory),DIVE_SIM_GALLERY=str(gallery),DIVE_SIM_APNEA_PERSISTENCE=f'{mode}:{choice}')
    result=subprocess.run([str(binary),'--stop_at=7','--no-rt'],env=env,text=True,capture_output=True,timeout=60)
    if evidence:(gallery/'native.txt').write_text(result.stdout+result.stderr)
    assert result.returncode==0,result.stdout+result.stderr
    assert f'APNEA PERSISTENCE {mode.upper()} PASS' in result.stdout,result.stdout
    return gallery
for choice in range(10):
    with tempfile.TemporaryDirectory(prefix='apnea-restart-') as tmp:
        directory=Path(tmp)
        sentinels={'preference.txt':b'DIVE_WATCH_GF 1 30 70\n','dive-17.log':b'Unrelated scuba log\x00\xff','recovery.checkpoint':bytes(range(256))}
        for name,data in sentinels.items():(directory/name).write_bytes(data)
        run(directory,'save',choice)
        # The baseline has no durable freediving preferences: red before adapter implementation.
        assert (directory/'apnea-preferences.txt').exists(),'Applied freediving preferences were not saved'
        run(directory,'load',choice)
        for name,data in sentinels.items():assert (directory/name).read_bytes()==data,f'Apnea changed unrelated {name}'
        assert not list(directory.glob('.apnea-pending-*')),'Temporary preference files leaked'
        print(f'RESTART PASS: water={choice//5}, bottom={choice%5}; unrelated GF/log/checkpoint bytes preserved')
# Corrupt schemas fall back through a fresh public UI process and remain untouched.
for bad in (b'',b'DIVE_WATCH_APNEA 2 1 2\n',b'DIVE_WATCH_APNEA 1 2 0\n',
            b'DIVE_WATCH_APNEA 1 0 5\n',b'DIVE_WATCH_APNEA 1 1 2\nextra',b'DIVE_WATCH_APNEA 1 0 0\n\x00'):
    with tempfile.TemporaryDirectory(prefix='apnea-corrupt-') as tmp:
        directory=Path(tmp);path=directory/'apnea-preferences.txt';path.write_bytes(bad)
        run(directory,'load',0)
        assert path.read_bytes()==bad,'Loading a bad preference overwrote the source'
print('INVALID PREFERENCE PASS: absent/default and six malformed/version/range/trailing-data files')
with tempfile.TemporaryDirectory(prefix='apnea-write-failure-') as tmp:
    directory=Path(tmp)
    run(directory,'save',1)
    previous=(directory/'apnea-preferences.txt').read_bytes()
    (directory/'capture-load').mkdir(exist_ok=True)
    directory.chmod(0o500)
    try:
        run(directory,'save',5)
        assert (directory/'apnea-preferences.txt').read_bytes()==previous,'Failed save changed the prior file'
        run(directory,'load',1)
    finally:directory.chmod(0o700)
print('WRITE FAILURE PASS: current applied UI choice remains usable, previous durable file survives and reloads')
# Supported direct native checks must not touch default interactive storage either.
with tempfile.TemporaryDirectory(prefix='apnea-direct-isolation-') as tmp:
    directory=Path(tmp);normal=directory/'dive-watch-simulator';normal.mkdir()
    target=normal/'apnea-preferences.txt';target.write_bytes(b'DIVE_WATCH_APNEA 1 1 2\n')
    gallery=directory/'gallery';gallery.mkdir()
    env=os.environ.copy();env.pop('DIVE_WATCH_STATE_DIR',None)
    for key in list(env):
        if key.startswith('DIVE_SIM_'):del env[key]
    env.update(SDL_VIDEODRIVER='dummy',XDG_STATE_HOME=str(directory),DIVE_SIM_APNEA_SETTINGS_CHECK='1',DIVE_SIM_GALLERY=str(gallery))
    result=subprocess.run([str(binary),'--stop_at=22','--no-rt'],env=env,text=True,capture_output=True,timeout=60)
    assert result.returncode==0 and 'APNEA SETTINGS PASS' in result.stdout,result.stdout+result.stderr
    assert target.read_bytes()==b'DIVE_WATCH_APNEA 1 1 2\n'
    assert sorted(p.name for p in normal.iterdir())==['apnea-preferences.txt']
print('DIRECT CHECK ISOLATION PASS: ignored normal preference; no normal files changed or created')
print('APNEA PERSISTENCE PASS: 20 separate UI processes, all ten combinations, cancel/phase lock, fresh sessions, live Main/Statistics agreement and independent scuba files')
