#!/usr/bin/env python3
"""Native storage boundary checks, separate from the process-restart UI journey."""
import ctypes,os,subprocess,tempfile
from pathlib import Path
src=Path(__file__).resolve().parents[1]/'src'
with tempfile.TemporaryDirectory(prefix='watch-preferences-test-') as tmp:
 root=Path(tmp);lib=root/'preferences.so'
 subprocess.run(['gcc','-shared','-fPIC','-Wall','-Wextra',str(src/'watch_preferences_host.c'),'-o',str(lib)],check=True)
 host=ctypes.CDLL(str(lib));load=host.sim_host_watch_preferences_load;save=host.sim_host_watch_preferences_save
 load.argtypes=[ctypes.POINTER(ctypes.c_int)]*3;save.argtypes=[ctypes.c_int]*3
 os.environ['DIVE_WATCH_STATE_DIR']=str(root/'state');file=root/'state/watch-preferences.txt'
 def read():
  values=[ctypes.c_int(-9) for _ in range(3)];code=load(*(ctypes.byref(v) for v in values));return code,tuple(v.value for v in values)
 assert read()==(1,(-9,-9,-9))
 for values in [(0,0,0),(1,6,4),(0,3,1)]:
  assert save(*values)==0 and read()==(0,values)
 prior=file.read_bytes()
 for values in [(-1,0,0),(2,0,0),(0,7,0),(0,0,5)]:
  assert save(*values)==-1 and file.read_bytes()==prior
 for invalid in [b'',b'DIVE_WATCH_FACES 2 0 0 0\n',b'DIVE_WATCH_FACES 1 0 7 0\n',b'DIVE_WATCH_FACES 1 0 0 5\n',b'DIVE_WATCH_FACES 1 0 0 0\nJUNK',b'DIVE_WATCH_FACES 1 0 0',b'X'*200]:
  file.write_bytes(invalid);assert read()==(-1,(-9,-9,-9))
 # A path through an ordinary file cannot be created; failed save preserves it.
 blocked=root/'blocked';blocked.write_text('unchanged');os.environ['DIVE_WATCH_STATE_DIR']=str(blocked/'child')
 assert save(0,0,0)==-1 and blocked.read_text()=='unchanged'
 print('WATCH PREFERENCES PASS: round trips, stable IDs, malformed/truncated files, untouched defaults and failed-write preservation')
