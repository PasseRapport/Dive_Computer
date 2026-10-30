# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent double-precision analytical oracle. See reference-provenance.json.
No production or policy imports. Public inputs are seconds and absolute bar.
"""
import math

N2_HALF_MIN = (5.,8.,12.5,18.5,27.,38.3,54.3,77.,109.,146.,187.,239.,305.,390.,498.,635.)
HE_HALF_MIN = (1.88,3.02,4.72,6.99,10.21,14.48,20.53,29.11,41.2,55.19,70.69,90.34,115.29,147.42,188.24,240.03)
N2_A_C = (1.1696,1.,.8618,.7562,.6200,.5043,.4410,.4000,.3750,.3500,.3295,.3065,.2835,.2610,.2480,.2327)
N2_A_B = (1.1696,1.,.8618,.7562,.6667,.5600,.4947,.4500,.4187,.3798,.3497,.3223,.2850,.2737,.2523,.2327)
N2_B = (.5578,.6514,.7222,.7825,.8126,.8434,.8693,.8910,.9092,.9222,.9319,.9403,.9477,.9544,.9602,.9653)
DURATIONS = (1,3,8,10,18,20,60,100,300,800,3600)
# Frozen before running either replay or reading source arrays. Single-segment
# comparisons permit binary32 inputs/state and the original poorly conditioned
# Schreiner evaluation. Bound covers cancellation at reciprocal k up to ~55,000 s.
SINGLE_ABS_BAR = 1.e-4
# Long histories amplify binary32 state rounding at each accepted sample.
HISTORY_ABS_BAR = 5.e-4
FACTOR_ABS_TOL = 1.e-10
COEFFICIENT_ABS_TOL = 1.e-7
K_ABS_TOL = 1.e-9


def exposure_factor(seconds, half_min):
    return -math.expm1(-math.log(2.) * seconds / (60. * half_min))


def segment(initial, start_bar, end_bar, seconds, half_min, fraction=.79):
    """Exact integral of dP/dt=k*(Pi(t)-P), stable for small k*t.
    q=1-exp(-x); ramp response=1-q/x (series avoids cancellation).
    Equivalent to Haldane (flat) and Schreiner (ramp); k is per second.
    """
    x = math.log(2.) * seconds / (60. * half_min)
    q = -math.expm1(-x)
    ramp = (x/2 - x*x/6 + x*x*x/24 - x**4/120 + x**5/720) if x < .001 else 1-q/x
    inspired_start = fraction*(start_bar-.0493)
    return initial + (inspired_start-initial)*q + fraction*(end_bar-start_bar)*ramp


def replay_reference(rows):
    tissue = [.7902*(rows[0][1]-.0493)]*16
    result = [tissue[:]]
    for (t0,p0),(t1,p1) in zip(rows, rows[1:]):
        tissue = [segment(v,p0,p1,t1-t0,h) for v,h in zip(tissue,N2_HALF_MIN)]
        result.append(tissue[:])
    return result
