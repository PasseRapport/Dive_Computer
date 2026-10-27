# SPDX-License-Identifier: GPL-3.0-or-later
"""The public replay comparison must reject altered guidance and missing rows."""
import copy
import unittest
from compare_schedule import compare_observation, compare_trace


class ComparisonTests(unittest.TestCase):
    def setUp(self):
        self.expected = dict(timestamp_seconds=60, absolute_pressure_bar=2., dive_seconds=60,
            nitrogen_bar=[1.]*16, helium_bar=[0.]*16, gf_anchor_ambient_bar=0.,
            gf_low_depth_bar=0., ndl_seconds=600, tts_seconds=0,
            stop_seconds=[0]*50, analytical_ceiling_meters=0., ceiling_meters=0.,
            availability=dict.fromkeys(('tissues','ndl','tts','ceiling','stops'),'available'))
        self.actual = copy.deepcopy(self.expected)

    def test_unchanged_output_passes(self):
        self.assertEqual(compare_observation(self.actual,self.expected), [])

    def test_guidance_and_availability_mutations_are_rejected(self):
        for field,value in [('ndl_seconds',660),('tts_seconds',1),
                            ('stop_seconds',[10]+[0]*49),('availability',{}),
                            ('nitrogen_bar',[float('nan')]+[1.]*15),
                            ('helium_bar',[.01]+[0.]*15),('ceiling_meters',1.)]:
            with self.subTest(field=field):
                changed = copy.deepcopy(self.actual)
                changed[field] = value
                self.assertTrue(compare_observation(changed,self.expected))

    def test_trace_requires_every_timestamp(self):
        self.assertTrue(compare_trace([], [self.expected]))
        altered = copy.deepcopy(self.actual)
        altered['timestamp_seconds'] = 61
        self.assertTrue(compare_trace([altered], [self.expected]))

if __name__ == '__main__':
    unittest.main()
