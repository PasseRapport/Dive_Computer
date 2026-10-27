# SPDX-License-Identifier: GPL-3.0-or-later
"""Self-check the independent reference against direct mathematical identities.

No replay executable or output is used to choose these expectations. Integration
comparisons are deliberately separate from this reference's own unit checks.
"""
import math
import unittest

from schedule_reference import (Reference, SURFACE, GF_HIGH, GF_LOW, STOP_BAR,
                                N2_A_C, N2_B, ceiling_pressure, exposure,
                                gradient_factor, ndl, plan, quantized_ceiling,
                                requires_decompression, stop_depth, travel)


class AnalyticalReferenceTests(unittest.TestCase):
    def test_one_half_time_is_arithmetic_midpoint(self):
        initial = (.4,)*16
        inspired = .79*(4.01325-.0493)
        result = exposure(initial, 4.01325, 4.01325, 300)
        self.assertAlmostEqual(result[0], (.4+inspired)/2, places=14)

    def test_linear_integral_has_segment_composition_property(self):
        start = (.76,)*16
        full = exposure(start, SURFACE, SURFACE+3, 120)
        split = exposure(exposure(start, SURFACE, SURFACE+1, 40),
                         SURFACE+1, SURFACE+3, 80)
        for a, b in zip(full, split):
            self.assertAlmostEqual(a, b, places=14)

    def test_ceiling_inverts_saturation_inequality(self):
        desired_pressure = 1.7
        for gf in (.3, .7, 1.):
            saturated = tuple(desired_pressure+gf*(a+desired_pressure/b-desired_pressure)
                              for a, b in zip(N2_A_C, N2_B))
            self.assertAlmostEqual(ceiling_pressure(saturated, gf), desired_pressure, places=14)

    def test_gf_anchor_interpolation_and_surface_rule(self):
        self.assertEqual(gradient_factor(SURFACE+.9, SURFACE, .9), GF_LOW)
        self.assertAlmostEqual(gradient_factor(SURFACE+.45, SURFACE, .9), .5)
        self.assertEqual(gradient_factor(SURFACE+.04, SURFACE, .9), GF_HIGH)
        self.assertEqual(gradient_factor(SURFACE+.6, SURFACE, 0.), GF_LOW)

    def test_grid_is_conservative_at_exact_and_intermediate_bounds(self):
        for gauge, expected in ((-.01,0), (.01,.3), (.3,.3), (.31,.6), (.61,.9)):
            # Use zero surface for exact .3 representation; adding/subtracting
            # atmospheric pressure can move an exact grid point by an ULP.
            self.assertAlmostEqual(stop_depth(gauge, 0.), expected)

    def test_travel_rounds_each_leg_at_declared_nine_meters_per_minute(self):
        state = (.76,)*16
        _, p, dt = travel(state, SURFACE+3, SURFACE+2, SURFACE)
        self.assertEqual(dt, 67)
        self.assertEqual(p, SURFACE+2)
        _, _, probe = travel(state, SURFACE+3, SURFACE+3-.333334, SURFACE)
        self.assertEqual(probe, 23)

    def test_nominal_three_meter_travel_is_exactly_twenty_seconds(self):
        state = (.76,)*16
        for p in (1.31325, 1.61325, 1.91325, 2.21325, 3.01325, 4.01325):
            _, _, seconds = travel(state, p, p-.3, SURFACE)
            self.assertEqual(seconds, 20)

    def test_shallow_equilibrium_has_capped_ndl_and_no_mandatory_stop(self):
        p = SURFACE+.2
        state = (.79*(p-.0493),)*16
        self.assertFalse(requires_decompression(state, p))
        self.assertEqual(ndl(state, p), 14400)
        schedule = plan(state, p)
        self.assertEqual(schedule.tts_seconds, 0)
        self.assertEqual(sum(schedule.stop_seconds), 0)

    def test_deco_schedule_is_wait_plus_travel_and_preserves_input(self):
        p = SURFACE+3
        start = (.7902*(SURFACE-.0493),)*16
        state = exposure(start, SURFACE, p, 120)
        state = exposure(state, p, p, 2100)
        before = tuple(state)
        schedule = plan(state, p)
        self.assertEqual(tuple(state), before)
        self.assertGreater(sum(schedule.stop_seconds), 0)
        self.assertEqual(schedule.tts_seconds, sum(schedule.stop_seconds)+schedule.travel_seconds)
        self.assertTrue(all(seconds % 10 == 0 for seconds in schedule.stop_seconds))
        self.assertGreater(schedule.anchor_ambient_bar, SURFACE)

    def test_ndl_refinement_returns_first_violating_minute(self):
        p = SURFACE+3
        state = exposure((.7902*(SURFACE-.0493),)*16, SURFACE, p, 120)
        result = ndl(state, p)
        self.assertLessEqual(result, 7200)
        self.assertEqual(result % 60, 0)
        self.assertFalse(requires_decompression(exposure(state, p, p, result-60), p))
        self.assertTrue(requires_decompression(exposure(state, p, p, result), p))

    def test_quantized_bound_brackets_analytical_ceiling(self):
        desired = SURFACE+.534
        state = tuple(desired+GF_HIGH*(a+desired/b-desired) for a,b in zip(N2_A_C,N2_B))
        result = quantized_ceiling(state, SURFACE, 30)
        self.assertLessEqual(result, 5.34)
        self.assertLess(5.34-result, .1)

    def test_availability_boundary_and_surface_continuity(self):
        reference = Reference()
        self.assertEqual(reference.observe()['availability']['tts'], 'unavailable')
        reference.advance(59, SURFACE+.2)
        self.assertEqual(reference.observe()['availability']['ndl'], 'unavailable')
        reference.advance(60, SURFACE+.2)
        self.assertEqual(reference.observe()['availability']['ndl'], 'limited')
        reference.advance(120, SURFACE)
        at_surface = reference.observe()
        self.assertEqual(at_surface['availability']['tts'], 'unavailable')
        reference.advance(720, SURFACE)
        self.assertEqual(reference.dive_seconds, 120)
        self.assertGreater(reference.tissues[0], .7902*(SURFACE-.0493))

    def test_persistent_anchor_and_read_isolation(self):
        reference = Reference()
        reference.advance(120, SURFACE+3)
        observation = reference.advance(2220, SURFACE+3)
        anchor = observation['gf_anchor_ambient_bar']
        observation['nitrogen_bar'][0] = -99
        self.assertGreater(reference.observe()['nitrogen_bar'][0], 0)
        reference.advance(2400, SURFACE)
        reference.advance(3600, SURFACE)
        self.assertGreaterEqual(reference.anchor, anchor)


if __name__ == '__main__':
    unittest.main()
