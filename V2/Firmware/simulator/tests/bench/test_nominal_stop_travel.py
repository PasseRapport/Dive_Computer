#!/usr/bin/env python3
"""Regression at the public bench seam for exact three-metre stop travel.

Expected schedules are calculated only by the frozen independent oracle; the
listed TTS values were independently confirmed using 65-digit tissue math.
"""
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent/'gf-reference'))
from configurable_reference import Reference

cases = [(1,100,6.,1920,6682), (1,100,3.,3720,3472),
         (1,100,6.,3720,21402), (1,1,6.,3720,170472), (100,100,6.,3720,16272)]
failures = []
for low,high,gauge_bar,seconds,independent_tts in cases:
    pressure = 1.01325+gauge_bar
    commands = f'fresh {low} {high}\nadvance 120 {pressure:.9f}\nadvance {seconds} {pressure:.9f}\n'
    process = subprocess.run([sys.argv[1]], input=commands, text=True, capture_output=True, check=True)
    actual = json.loads(process.stdout.splitlines()[-1])
    reference = Reference(gf_low=low,gf_high=high)
    reference.advance(120,pressure)
    expected = reference.advance(seconds,pressure)
    assert expected['tts_seconds'] == independent_tts
    label = f'GF {low}/{high}, {gauge_bar*10:.0f}m, t={seconds}'
    if actual['stop_seconds'] != expected['stop_seconds']:
        failures.append(f'{label}: exact mandatory-stop slots differ')
    difference = actual['tts_seconds']-independent_tts
    if abs(difference)>3:
        failures.append(f'{label}: TTS {actual["tts_seconds"]} vs {independent_tts} ({difference:+d}s; allowed ±3s)')
    print(f'{label}: actual TTS={actual["tts_seconds"]}, expected={independent_tts}, delta={difference:+d}s')
if failures:
    print('\n'.join(failures),file=sys.stderr)
    sys.exit(1)
print('EXACT STOP TRAVEL PASS: five independent schedules; exact stop slots, unchanged TTS-only tolerance')
