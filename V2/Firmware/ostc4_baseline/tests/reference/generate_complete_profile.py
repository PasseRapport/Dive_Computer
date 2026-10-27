# SPDX-License-Identifier: GPL-3.0-or-later
"""Additional predeclared fixed exposure with long holds through full clearance.

These deliberately generous holds are exposure inputs, not expected mandatory
stops. Their results are calculated by both engines; no output is fed back.
"""
from pathlib import Path
from generate_profiles import PROFILES, generate

PROFILES['full-stop-progression'] = [(0,0),(120,30),(2220,30),
    (2340,12),(2940,12),(2960,9),(3560,9),(3580,6),(4780,6),
    (4800,3),(7200,3),(7220,0),(7520,0)]
if __name__ == '__main__':
    generate(Path(__file__).resolve().parent/'profiles')
