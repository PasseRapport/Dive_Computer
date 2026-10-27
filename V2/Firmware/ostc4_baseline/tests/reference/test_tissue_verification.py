# SPDX-License-Identifier: GPL-3.0-or-later
"""Public boundary integration regression tests; no production imports.
python3 test_tissue_verification.py BASELINE CORRECTED [SOURCE CORRECTED_SOURCE]
"""
import hashlib, json, math, pathlib, sys, tempfile, unittest
from tissue_reference import *
from verify_tissue_replay import profiles, run, analytical_checks, differences, cadence_check
from audit_tissue_tables import audit

BASELINE=pathlib.Path(sys.argv.pop(1))
CORRECTED=pathlib.Path(sys.argv.pop(1))
SOURCES=[]
while len(sys.argv)>1 and not sys.argv[1].startswith('-'):SOURCES.append(sys.argv.pop(1))


class OracleProperties(unittest.TestCase):
    def test_frozen_oracle_hash(self):
        here=pathlib.Path(__file__).parent
        frozen=json.loads((here/'reference-provenance.json').read_text())
        self.assertEqual(hashlib.sha256((here/'tissue_reference.py').read_bytes()).hexdigest(), frozen['oracle_sha256'])
    def test_one_halftime_halves_residual_all_compartments(self):
        for half in N2_HALF_MIN+HE_HALF_MIN:
            self.assertAlmostEqual(exposure_factor(half*60,half),.5,places=15)
            self.assertAlmostEqual(segment(.5,4.,4.,half*60,half),(.5+.79*(4.-.0493))/2,places=14)
    def test_ramp_semigroup_both_signs_all_compartments(self):
        for h in N2_HALF_MIN:
            for p0,p1 in ((1.,7.),(7.,1.)):
                whole=segment(.75,p0,p1,100,h)
                split=segment(segment(.75,p0,(p0+p1)/2,50,h),(p0+p1)/2,p1,50,h)
                self.assertAlmostEqual(whole,split,places=13)
    def test_small_time_ramp_response_matches_direct_numerical_integral(self):
        # Independent midpoint quadrature of convolution integral, no Schreiner.
        for h in N2_HALF_MIN:
            k=math.log(2)/(h*60);steps=10000;dt=1/steps
            integral=sum(k*.79*(1+6*((i+.5)*dt)-.0493)*math.exp(-k*(1-(i+.5)*dt))*dt for i in range(steps))
            expected=.75*math.exp(-k)+integral
            self.assertAlmostEqual(segment(.75,1.,7.,1,h),expected,places=11)


class PublicReplay(unittest.TestCase):
    def test_all_corrected_replays_against_independent_reference(self):
        with tempfile.TemporaryDirectory() as temp:
            for name,rows in profiles().items():
                with self.subTest(profile=name):
                    metadata,actual,raw=run(CORRECTED,rows,pathlib.Path(temp),name)
                    self.assertEqual(metadata['correction_set'],'cached-10s-and-stable-schreiner-v1')
                    self.assertEqual(metadata['policy'],'ostc4-beta-7198d20-tissue-corrected-air-gf30-70-v1')
                    checked=analytical_checks(rows,actual)
                    self.assertTrue(checked['pass'],str(checked['violations'][:5]))
    def test_original_defect_is_visible_through_public_interface(self):
        with tempfile.TemporaryDirectory() as temp:
            for name in ('uptake-10','elimination-10','repetitive-cadence-10','ramp-short-extreme'):
                with self.subTest(profile=name):
                    rows=profiles()[name]; md,actual,raw=run(BASELINE,rows,pathlib.Path(temp),name)
                    self.assertEqual(md['correction_set'],'none')
                    self.assertEqual(md['policy'],'ostc4-beta-7198d20-unchanged-air-gf30-70-v1')
                    self.assertFalse(analytical_checks(rows,actual)['pass'])
    def test_cadence_equivalence_at_common_timestamps(self):
        with tempfile.TemporaryDirectory() as temp:
            runs={}
            for cadence in (10,20,60):
                name=f'repetitive-cadence-{cadence}'
                md,actual,raw=run(CORRECTED,profiles()[name],pathlib.Path(temp),name)
                runs[cadence]=actual
            checked=cadence_check(runs)
            self.assertTrue(checked['pass'],str(checked))
    def test_coefficient_and_all_cached_factors(self):
        if len(SOURCES)!=2:self.skipTest('provide original and generated corrected source paths for source audit')
        original=audit(SOURCES[0]);corrected=audit(SOURCES[1])
        self.assertEqual(len(original['checks']),448)
        self.assertEqual(len(original['failures']),4)
        self.assertFalse(corrected['failures'])
        self.assertEqual({(v['gas'],v['compartment']) for v in original['failures']},{('N2',5),('N2',14),('He',5),('He',14)})

if __name__=='__main__': unittest.main()
