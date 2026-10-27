# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent public-CLI continuity and prediction-isolation acceptance check.

Usage: python3 test_continuity_reference.py REPLAY [--output REPORT.json]
REPLAY accepts a profile path and optional none/repeated/interleaved mode.
No production implementation or policy data is imported or inspected.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

from tissue_reference import HISTORY_ABS_BAR, replay_reference

HERE = Path(__file__).resolve().parent
PROFILE = HERE / 'repetitive.csv'
ROWS = ((0, 1.01325), (120, 4.01325), (1920, 4.01325),
        (2100, 1.01325), (3600, 1.01325), (3720, 3.01325),
        (4920, 3.01325), (5040, 1.01325))
MODES = ('none', 'repeated', 'interleaved')
# Declared before any ticket03 production implementation/output was inspected.
TOLERANCE_BAR = HISTORY_ABS_BAR  # 0.0005 absolute bar, whole-history bound.


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def run(executable, mode=None):
    command = [str(executable), str(PROFILE)]
    if mode is not None:
        command.append(mode)
    completed = subprocess.run(command, text=True, capture_output=True, check=True)
    parsed = [json.loads(line) for line in completed.stdout.splitlines() if line.strip()]
    assert parsed and parsed[0]['type'] == 'metadata', 'first row must be metadata'
    assert sum(row['type'] == 'metadata' for row in parsed) == 1
    observed = [row for row in parsed if row['type'] == 'observation']
    assert len(observed) == len(ROWS), 'one observation is required at every sample'
    assert len(parsed) == 1 + len(ROWS), 'unexpected JSON row type'
    for actual, (seconds, pressure) in zip(observed, ROWS):
        assert actual['timestamp_seconds'] == seconds
        assert len(actual['nitrogen_bar']) == 16
        assert len(actual['helium_bar']) == 16
        assert all(math.isfinite(value) for value in actual['nitrogen_bar'])
        assert all(value == 0 for value in actual['helium_bar'])
    return {'metadata': parsed[0], 'observations': observed,
            'stdout_sha256': hashlib.sha256(completed.stdout.encode()).hexdigest()}


def verify(executable):
    runs = {mode: run(executable, mode) for mode in MODES}
    runs['default'] = run(executable)
    baseline = runs['none']['observations']
    # Compare whole dictionaries: tissues and every schedule/GF/availability field.
    # Metadata may legitimately identify the invocation mode and is not compared.
    for mode, result in runs.items():
        assert result['observations'] == baseline, f'{mode} mutated observable exposure state'

    reference = replay_reference(ROWS)
    points = []
    for row, actual, expected in zip(ROWS, baseline, reference):
        errors = [a - e for a, e in zip(actual['nitrogen_bar'], expected)]
        assert max(map(abs, errors)) <= TOLERANCE_BAR, (row[0], errors)
        points.append({'timestamp_seconds': row[0], 'nitrogen_error_bar': errors})

    tissue = [row['nitrogen_bar'] for row in baseline]
    initial = tissue[0]
    # Surface interval must desaturate while retaining residual nitrogen.
    assert all(start > end > reset for start, end, reset in zip(tissue[3], tissue[4], initial))
    # All compartments take up nitrogen during the second bottom interval.
    assert all(end > start for start, end in zip(tissue[5], tissue[6]))
    # The second dive must inherit tissue loading from dive 1. Compare its final
    # tissues against a genuinely fresh exposure at the same surface pressure.
    fresh_second = replay_reference(tuple((t - 3600, p) for t, p in ROWS[4:]))[-1]
    assert all(actual > fresh + TOLERANCE_BAR for actual, fresh in zip(tissue[-1], fresh_second))
    return {
        'pass': True,
        'scope': 'Synthetic host tissue continuity and observable prediction isolation only; no independent schedule acceptance or hardware validation.',
        'tolerance_bar': TOLERANCE_BAR,
        'max_absolute_error_bar': max(abs(e) for point in points for e in point['nitrogen_error_bar']),
        'profile_sha256': sha(PROFILE),
        'reference_sha256': sha(HERE / 'tissue_reference.py'),
        'executable_sha256': sha(executable),
        'reference_provenance': json.loads((HERE / 'reference-provenance.json').read_text()),
        'runs': runs,
        'analytical_points': points,
        'residual_nitrogen_at_second_descent_bar': [value - reset for value, reset in zip(tissue[4], initial)],
        'second_bottom_uptake_bar': [end - start for start, end in zip(tissue[5], tissue[6])],
        'final_excess_over_fresh_second_dive_bar': [actual - fresh for actual, fresh in zip(tissue[-1], fresh_second)],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = verify(args.executable.resolve())
    if args.output:
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('pass', 'tolerance_bar', 'max_absolute_error_bar')}, indent=2))


if __name__ == '__main__':
    main()
