# SPDX-License-Identifier: GPL-3.0-or-later
"""Fixed synthetic pressure inputs; holds are inputs, never expected stops."""
import csv
from pathlib import Path

SURFACE = 1.01325
# Seconds and gauge depth metres. Explicit waypoints override regular 30s samples.
PROFILES = {
    'shallow-boundaries': [(0,0),(30,5),(59,5),(60,5),(61,5),(660,5),(720,0),(900,0)],
    'onset-20m': [(0,0),(120,20),(3720,20),(3854,0),(4154,0)],
    'demo-30m-35min-fixed-holds': [(0,0),(120,30),(2220,30),(2380,6),(2620,6),(2640,3),(3540,3),(3560,0),(3860,0)],
    'violation-30m': [(0,0),(120,30),(2220,30),(2420,0),(2720,0)],
    'repetitive': [(0,0),(120,30),(2220,30),(2380,6),(2620,6),(2640,3),(3540,3),(3560,0),(5360,0),(5480,20),(6680,20),(6814,0),(7114,0)],
}


def generate(directory):
    directory.mkdir(parents=True, exist_ok=True)
    for name, waypoints in PROFILES.items():
        rows = [waypoints[0]]
        for (t0,d0),(t1,d1) in zip(waypoints,waypoints[1:]):
            for t in sorted(set(range(t0+30,t1,30)) | {t1}):
                rows.append((t, d0+(d1-d0)*(t-t0)/(t1-t0)))
        with (directory / (name+'.csv')).open('w',newline='') as stream:
            writer = csv.writer(stream, lineterminator='\n')
            writer.writerow(('timestamp_seconds','absolute_pressure_bar'))
            writer.writerows((t,f'{SURFACE+d*.1:.9f}') for t,d in rows)

if __name__ == '__main__':
    generate(Path(__file__).resolve().parent/'profiles')
