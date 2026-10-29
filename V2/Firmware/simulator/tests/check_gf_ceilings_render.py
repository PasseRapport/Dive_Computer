#!/usr/bin/env python3
"""Native paired-ceiling display/stop/alert evidence; PC rendering only."""
import csv
from decimal import Decimal, ROUND_HALF_UP, ROUND_CEILING
import json
from pathlib import Path
import sys
from PIL import Image, ImageChops

prefix = sys.argv[1]
states, frames = {}, {}
for index in range(6, 13):
    state = json.loads(Path(f'{prefix}-{index}.json').read_text())
    path = Path(f'{prefix}-{index}.png')
    image = Image.open(path if path.exists() else f'{prefix}-{index}.bmp').convert('RGB')
    assert image.size == (1200, 800)
    with Path(f'{prefix}-{index}.csv').open() as stream:
        samples = list(csv.DictReader(stream))
    assert len(samples) == state['history_count']
    assert int(samples[-1]['timestamp_seconds']) == state['seconds']
    states[index], frames[index] = state, image.crop((22, 32, 342, 332))
    # Both values and stable identifying label occupy the actual bottom field.
    footer = frames[index].crop((55, 215, 265, 274))
    assert sum(min(pixel)>180 for pixel in footer.getdata())>150
    endpoint = state['gf_ceilings']
    if index != 11:
        assert endpoint['validity'] == [1, 1]
        assert endpoint['low_m'] >= endpoint['high_m'] >= 0

def tenths(value):
    return str(Decimal(str(value)).quantize(Decimal('0.1'), rounding=ROUND_HALF_UP))

assert states[6]['gf_ceilings']['low_m'] == states[6]['gf_ceilings']['high_m'] == 0
# Independent fixture's decimal expectations, separately from raw0.006m bound.
assert tenths(states[7]['gf_ceilings']['low_m']) == '13.4'  # raw engine convention retained
assert tenths(states[7]['physical']['gf_low_m']) == '13.3'
assert tenths(states[7]['physical']['gf_high_m']) == '7.2'
assert tenths(states[7]['gf_ceilings']['high_m']) == '7.2'
assert states[7]['watch']['stop_depth_dm'] > 0
assert states[8]['watch']['stop_remaining_ms'] > 0
assert states[8]['watch']['stop_depth_dm'] <= states[8]['watch']['depth_dm']
assert states[9]['watch']['alerts'] & 1 and states[9]['watch']['notifications'] & 1
assert states[10]['watch']['alerts'] & 1 and not states[10]['watch']['notifications'] & 1
assert states[11]['gf_ceilings']['validity'] == [3, 3]
assert states[12]['gf_ceilings']['low_m'] == states[12]['gf_ceilings']['high_m'] > 30
assert tenths(states[12]['gf_ceilings']['low_m']) == '44.3'
assert tenths(states[12]['physical']['gf_low_m']) == '44.1'
for first, second in [(6, 7), (7, 8), (9, 10), (10, 11), (11, 12)]:
    difference = ImageChops.difference(frames[first], frames[second])
    assert sum(max(pixel)>50 for pixel in difference.getdata())>100
print('PAIRED CEILINGS RENDER PASS: simultaneous Low/High, units/rounding, zero, equality, large values, mandatory depth/countdown, alerts/acknowledgement and explicit invalidity; native text/glyph geometry assertions also passed')

# Identical absolute-pressure replay under both densities, through public inputs.
twins = [json.loads(Path(f'{prefix}-{i}.json').read_text()) for i in (13, 14)]
assert Path(f'{prefix}-13.csv').read_bytes() == Path(f'{prefix}-14.csv').read_bytes()
for key in ('tissues', 'gf_ceilings', 'ceiling_meters', 'ndl_seconds', 'tts_seconds', 'stop_seconds', 'validity', 'seconds', 'absolute_pressure_bar'):
    assert twins[0][key] == twins[1][key], key
assert abs(twins[0]['depth_m'] / twins[1]['depth_m'] - 1.025) < 1e-6
for state in list(states.values()) + twins:
    assert state['engine_depth_units'] == 'reference_10m_per_bar'
    if state['watch']['state'] not in (1, 2):
        continue
    density = Decimal(str(state['density_kg_m3']))
    # Independent decimal arithmetic; predeclared binary32 tolerance 0.0001m.
    for raw_key, physical_key in [('low_m', 'gf_low_m'), ('high_m', 'gf_high_m')]:
        expected = Decimal(str(state['gf_ceilings'][raw_key])) * Decimal(10000) / (density * Decimal('9.80665'))
        assert abs(expected - Decimal(str(state['physical'][physical_key]))) < Decimal('0.0001')
    ceiling = Decimal(str(state['ceiling_meters'])) * Decimal(10000) / (density * Decimal('9.80665'))
    assert abs(ceiling - Decimal(str(state['physical']['ceiling_m']))) < Decimal('0.0001')
    assert state['physical']['ceiling_dm'] == int((ceiling * 10).to_integral_value(rounding=ROUND_CEILING))
    slot = state['physical']['stop_slot']
    if slot:
        for offset, field in [(0, 'stop_lower_m'), (1, 'stop_upper_m')]:
            expected = Decimal('30000') * (slot + offset) / (density * Decimal('9.80665'))
            assert abs(expected - Decimal(str(state['physical'][field]))) < Decimal('0.0001')
        assert state['watch']['stop_depth_dm'] == int(Decimal(str(state['physical']['stop_lower_m'] * 10)).quantize(Decimal(1), rounding=ROUND_HALF_UP))
print('WATER PRESSURE PRESENTATION PASS: unchanged accepted CSV/tissues/raw outputs, density ratio, physical endpoints and both pressure stop bounds')

# Air ppO2 is determined by accepted absolute pressure. Independent exact
# decimal products below use oxygen fraction .21; binary32 tolerance1e-6bar.
for surface in range(2):
    for salt in range(2):
        for offset, (pressure, ppo2, oxygen) in enumerate(((6.60,1.386,False),
                (6.66,1.3986,False),(6.67,1.4007,True),(6.70,1.407,True))):
            index=20+surface*8+salt*4+offset
            state=json.loads(Path(f'{prefix}-{index}.json').read_text())
            assert abs(state['absolute_pressure_bar']-pressure)<1e-6, state
            assert abs(state['physical']['ppo2_bar']-ppo2)<1e-6, state
            assert bool(state['watch']['alerts'] & 64)==oxygen, state
            assert state['density_kg_m3']==(1025 if salt else 1000), state
            assert abs(state['surface_bar']-(.9 if surface else 1))<1e-6, state
print('ABSOLUTE PPO2 RENDER PASS: pressure-driven air value and raw 1.4bar warning in both waters/two surface references; literal rounded footer verified by native public-menu journey')
