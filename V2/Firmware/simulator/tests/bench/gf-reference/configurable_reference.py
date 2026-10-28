# SPDX-License-Identifier: GPL-3.0-or-later
"""Independent binary64 air/GF schedule reference for synthetic host tests.

Written as an analytical state-transition model, not a binding to a dive engine.
This GF extension parameterizes the existing independently authored model.
The only imported numerical implementation is the separately authored analytical
tissue integral. Policy facts were read from the pinned source and policy record;
no C code is compiled, called, imported or generated here.

The policy is deliberately unusual: travel uses 600/9 s/bar and ceil per leg;
initial ascent probes use 1 bar and 0.333334 bar; a stop means a failed projected
ascent followed by a ten-second wait. NDL is the first failing minute below the
refinement limit. This is a matched-policy test reference, not diving guidance.
Binary32 roundoff, cached factor defects and numerical cancellation are NOT
reproduced. Sharp scheduling boundaries can therefore reveal discrepancies.
"""
from copy import deepcopy
from dataclasses import dataclass
from fractions import Fraction
import math

from tissue_reference import N2_A_C, N2_B, N2_HALF_MIN, segment

SURFACE = 1.01325
GF_LOW, GF_HIGH = .30, .70


@dataclass(frozen=True)
class GFPair:
    low_percent: int = 30
    high_percent: int = 70

    def __post_init__(self):
        if (type(self.low_percent) is not int or type(self.high_percent) is not int
                or not 1 <= self.low_percent <= self.high_percent <= 100):
            raise ValueError('GF pair requires integers 1 <= low <= high <= 100')

    @property
    def low(self):
        return self.low_percent / 100.

    @property
    def high(self):
        return self.high_percent / 100.


DEFAULT_PAIR = GFPair()
PROBE_BAR = .333334
SECONDS_PER_BAR = 600./9  # ascentRate_meterperminute is FLOAT, not integer
STOP_BAR = .3
CAP_SECONDS = 59940


def exposure(tissues, start_bar, end_bar, seconds):
    """Solve all 16 independent first-order N2 ODEs on a linear segment."""
    if seconds == 0:
        return tuple(tissues)
    return tuple(segment(v, start_bar, end_bar, seconds, half)
                 for v, half in zip(tissues, N2_HALF_MIN))


def ceiling_pressure(tissues, gf):
    """Solve T <= P + gf*(a + P/b - P), then intersect compartments."""
    return max((t - gf*a) / (1 + gf*(1/b - 1))
               for t, a, b in zip(tissues, N2_A_C, N2_B))


def gradient_factor(pressure, surface, anchor_depth, pair=DEFAULT_PAIR):
    depth = pressure - surface
    if depth <= .05:
        return pair.high
    if depth >= anchor_depth:
        return pair.low
    return pair.high - (pair.high-pair.low)*depth/anchor_depth


def stop_depth(ceiling, surface):
    """Least declared stop at or deeper than the ambient ceiling."""
    return min(50, max(0, math.ceil((ceiling-surface)/STOP_BAR))) * STOP_BAR


def travel_seconds(pressure, target):
    """Exact decimal pressure timing for the declared nine-decimal CSV inputs.

    Input pressures and policy steps (1, .333334 and .3 bar) have at most nine
    decimal places. Recover their exact decimal values for integer-second ceil;
    binary64 arithmetic must not turn a nominal 20-second leg into 21 seconds.
    Tissue integrals remain binary64. This is a mathematical timing rule, not
    emulation of production's binary32 pressure arithmetic.
    """
    start = Fraction(str(round(pressure, 9)))
    end = Fraction(str(round(target, 9)))
    return math.ceil((start-end)*Fraction(600, 9))


def travel(tissues, pressure, target, surface):
    """One pressure leg, rounded upward to integer travel seconds."""
    if abs(pressure-surface) < .05:
        return tuple(tissues), surface, 0
    target = max(surface, target)
    seconds = travel_seconds(pressure, target) if pressure-target > .0001 else 0
    return exposure(tissues, pressure, target, seconds), target, seconds


def requires_decompression(tissues, pressure, surface=SURFACE, pair=DEFAULT_PAIR):
    """High-GF staged ascent feasibility predicate used by declared NDL policy.

    The probe ends above the surface; the surface tolerance is evaluated on its
    resulting tissue state. It is not an exact continuous direct ascent test.
    """
    state, p = tuple(tissues), pressure
    limit = ceiling_pressure(state, pair.high)
    if surface+stop_depth(limit, surface) < p-.15:
        for step in (1., PROBE_BAR):
            while p-step > surface and limit < (p-step if step == 1. else p):
                state, p, _ = travel(state, p, p-step, surface)
                limit = ceiling_pressure(state, pair.high)
    return limit > surface


def ndl(tissues, pressure, surface=SURFACE, pair=DEFAULT_PAIR):
    """Return matched-policy NDL seconds, including first-violating-minute rule."""
    coarse = 14400
    for seconds in range(600, 14401, 600):
        loaded = exposure(tissues, pressure, pressure, seconds)
        if requires_decompression(loaded, pressure, surface, pair):
            coarse = seconds if seconds == 14400 else seconds-600
            break
    if coarse > 7200:
        return coarse
    for seconds in range(coarse+60, coarse+601, 60):
        loaded = exposure(tissues, pressure, pressure, seconds)
        if requires_decompression(loaded, pressure, surface, pair):
            return seconds
    return coarse+600


@dataclass(frozen=True)
class Schedule:
    ndl_seconds: int
    tts_seconds: int
    stop_seconds: tuple
    anchor_ambient_bar: float
    low_depth_bar: float
    travel_seconds: int
    final_gf: float = GF_HIGH


def plan(tissues, pressure, surface=SURFACE, anchor=0., low_depth=0., dive_seconds=60, pair=DEFAULT_PAIR):
    """Project finite ascent states without changing tracked exposure tissues.

    Persistent anchor is the maximum scheduled first-stop ambient pressure.
    The interpolation depth uses that maximum once the actual diver reaches
    within 0.15 bar of it, otherwise it uses the newly projected first stop.
    The projected route probes coarse legs, lands on the rounded first stop,
    then chooses the earliest ten-second wait permitting each next-stop leg.
    """
    stops = [0]*50
    if dive_seconds < 60:
        return Schedule(0, 0, tuple(stops), anchor, low_depth, 0, pair.low)
    if not requires_decompression(tissues, pressure, surface, pair):
        return Schedule(ndl(tissues, pressure, surface, pair), 0, tuple(stops), anchor, low_depth, 0, pair.high)

    state, p, elapsed = tuple(tissues), pressure, 0
    gf = gradient_factor(p, surface, low_depth, pair)
    limit = ceiling_pressure(state, gf)
    first = stop_depth(limit, surface)
    # Explore a descending pressure route. A candidate beyond its rounded
    # ceiling is rejected; its ceiling still selects the first-stop grid point.
    if surface+first < p-.15:
        if surface+first < p-1.:
            while True:
                state, p, dt = travel(state, p, p-1., surface)
                elapsed += dt
                limit = ceiling_pressure(state, gf)
                if not (dt > 0 and p-1. > surface and limit < p-1.):
                    break
        while True:
            candidate, candidate_p, dt = travel(state, p, p-PROBE_BAR, surface)
            limit = ceiling_pressure(candidate, gf)
            first = stop_depth(limit, surface)
            if surface+first > candidate_p:
                break
            state, p = candidate, candidate_p
            elapsed += dt
            if not (dt > 0 and surface+first < p):
                break

    if limit <= surface and first == 0:
        return Schedule(0, 0, tuple(stops), anchor, low_depth, 0, gf)
    state, p, dt = travel(state, p, surface+first, surface)
    elapsed += dt
    travel_total = elapsed
    prior_anchor = anchor
    anchor = max(anchor, surface+first)
    low_depth = anchor-surface if prior_anchor >= pressure-.15 else first

    index = round(first/STOP_BAR)-1
    while index >= 0:
        target = surface+index*STOP_BAR
        gf = gradient_factor(target, surface, low_depth, pair)
        # Check a future leg before committing it. If unsafe, remain at the
        # current stop for one time quantum and retry the same leg.
        while True:
            candidate, candidate_p, dt = travel(state, p, target, surface)
            if ceiling_pressure(candidate, gf) <= target or stops[index] >= CAP_SECONDS:
                state, p = candidate, candidate_p
                elapsed += dt
                travel_total += dt
                break
            state = exposure(state, p, p, 10)
            stops[index] += 10
            elapsed += 10
        index -= 1
    return Schedule(0, elapsed, tuple(stops), anchor, low_depth, travel_total, gf)


def quantized_ceiling(tissues, surface, maximum_depth_m, gf=GF_HIGH):
    """Lower endpoint of a <0.01-bar bracket, using declared public bounds.

    Analytical bound is also exposed separately by Reference for auditability.
    A violated dive may place the true bound outside the bracket; this mirrors
    the observable bracket policy, and does not imply a valid mathematical root.
    """
    low, high = surface, 1.+maximum_depth_m/10
    bound = ceiling_pressure(tissues, gf)
    while high-low >= .01:
        mid = (low+high)/2
        if mid > bound:
            high = mid
        else:
            low = mid
    return (low-surface)*10


class ConfigurableReference:
    """Explicit fresh initialization and continuous accepted-sample history."""
    def __init__(self, surface_bar=SURFACE, timestamp_seconds=0, gf_low=30, gf_high=70):
        self.pair = GFPair(gf_low, gf_high)
        self.surface = surface_bar
        self.timestamp = timestamp_seconds
        self.pressure = surface_bar
        self.tissues = (.7902*(surface_bar-.0493),)*16
        self.anchor = self.low_depth = self.maximum_depth = 0.
        self.dive_seconds = 0
        self.result = self._observation(Schedule(0, 0, (0,)*50, 0., 0., 0))

    def _observation(self, schedule):
        available = self.dive_seconds >= 60 and self.pressure > self.surface and (schedule.ndl_seconds or schedule.tts_seconds)
        availability = dict.fromkeys(('ndl', 'tts', 'ceiling', 'stops'), 'available' if available else 'unavailable')
        availability['tissues'] = 'available'
        if available:
            if schedule.ndl_seconds >= 14400:
                availability['ndl'] = 'limited'
            if schedule.tts_seconds >= CAP_SECONDS:
                availability.update(dict.fromkeys(('tts', 'ceiling', 'stops'), 'limited'))
            if max(schedule.stop_seconds) >= CAP_SECONDS:
                availability['stops'] = 'limited'
        return dict(gf_pair=[self.pair.low_percent, self.pair.high_percent],
                    gf_low_ceiling_meters=max(0., (ceiling_pressure(self.tissues, self.pair.low)-self.surface)*10),
                    gf_high_ceiling_meters=max(0., (ceiling_pressure(self.tissues, self.pair.high)-self.surface)*10),
                    timestamp_seconds=self.timestamp, absolute_pressure_bar=self.pressure,
                    dive_seconds=self.dive_seconds, nitrogen_bar=list(self.tissues),
                    tissues_n2_bar=list(self.tissues), helium_bar=[0.]*16,
                    gf_anchor_ambient_bar=self.anchor, gf_low_depth_bar=self.low_depth,
                    ndl_seconds=schedule.ndl_seconds, tts_seconds=schedule.tts_seconds,
                    stop_seconds=list(schedule.stop_seconds), availability=availability,
                    ceiling_meters=quantized_ceiling(self.tissues, self.surface, self.maximum_depth, schedule.final_gf) if schedule.tts_seconds else 0.,
                    analytical_ceiling_meters=max(0., (ceiling_pressure(self.tissues, schedule.final_gf)-self.surface)*10),
                    travel_seconds=schedule.travel_seconds,
                    normalized_tts_seconds=schedule.tts_seconds or max(0, travel_seconds(self.pressure, self.surface)))

    def observe(self):
        return deepcopy(self.result)

    def advance(self, timestamp_seconds, pressure_bar):
        if timestamp_seconds == self.timestamp and pressure_bar == self.pressure:
            return self.observe()
        seconds = timestamp_seconds-self.timestamp
        if not 0 < seconds <= 3600 or not math.isfinite(pressure_bar) or not self.surface <= pressure_bar <= self.surface+6:
            raise ValueError('invalid timestamp interval or pressure')
        self.tissues = exposure(self.tissues, self.pressure, pressure_bar, seconds)
        if self.pressure > self.surface or pressure_bar > self.surface:
            self.dive_seconds += seconds
        self.timestamp, self.pressure = timestamp_seconds, pressure_bar
        self.maximum_depth = max(self.maximum_depth, (pressure_bar-self.surface)*10)
        schedule = plan(self.tissues, pressure_bar, self.surface, self.anchor, self.low_depth, self.dive_seconds, self.pair)
        self.anchor, self.low_depth = schedule.anchor_ambient_bar, schedule.low_depth_bar
        self.result = self._observation(schedule)
        return self.observe()


    def set_gf_pair(self, low, high):
        """Change policy only; rerun planning with the existing persistent anchors.

        This reference covers calculation policy, not recorded-dive UI locking.
        A normal replan can grow the persistent anchor; it can never clear it.
        No tissue integration or elapsed-time update is performed here.
        """
        pair = GFPair(low, high)  # validate complete pair before mutation
        schedule = plan(self.tissues, self.pressure, self.surface,
                        self.anchor, self.low_depth, self.dive_seconds, pair)
        self.pair = pair
        self.anchor, self.low_depth = schedule.anchor_ambient_bar, schedule.low_depth_bar
        self.result = self._observation(schedule)
        return self.observe()


Reference = ConfigurableReference
