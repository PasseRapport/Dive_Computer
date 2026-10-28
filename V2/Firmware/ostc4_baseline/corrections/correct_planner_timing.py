# SPDX-License-Identifier: GPL-3.0-or-later
"""Apply the explicit stop-grid timing correction to one pinned contained copy.

The upstream tree and both fixed comparison variants remain unchanged. Only the
continuous-exposure build selects this generated planner. No epsilon snapping or
rounding of arbitrary pressure inputs is introduced.
"""
import hashlib
import json
from pathlib import Path
import sys
source_path, output_path = map(Path, sys.argv[1:3])
policy = json.loads((Path(__file__).parent/'planner-timing-corrections.json').read_text())
original = source_path.read_bytes()
if hashlib.sha256(original).hexdigest() != policy['contained_source_sha256']:
    raise SystemExit('refusing to correct an unrecognized contained planner')
source = original.decode('latin1')

def replace(old, new, count):
    global source
    if source.count(old) != count:
        raise SystemExit('unexpected planner timing correction boundary: '+old)
    source = source.replace(old,new)

replace('float pressure_decrease)', 'float pressure_decrease, int nominal_three_metre_grid_leg)', 2)
for pressure,count in [('PRESSURE_TEN_METER',2),('PRESSURE_THREE_METER',2),('pressure_delta',1)]:
    replace(f'ascend_with_all_gaschanges(pDiveSettings, {pressure})',
            f'ascend_with_all_gaschanges(pDiveSettings, {pressure}, 0)',count)
replace('ascend_with_all_gaschanges(pDiveSettings, gStop.depth - next_depth)',
    '''ascend_with_all_gaschanges(pDiveSettings, gStop.depth - next_depth,
                pDiveSettings->last_stop_depth_bar == 0.3f &&
                pDiveSettings->input_second_to_last_stop_depth_bar == 0.6f &&
                pDiveSettings->input_next_stop_increment_depth_bar == 0.3f &&
                pDiveSettings->decogaslist[1].change_during_ascent_depth_meter_otherwise_zero == 0)''',1)
replace('time_for_ascend = (int)ceilf(pressure_difference * ascendrate_in_seconds_for_one_bar);',
    '''/* Exact declared grid height avoids a spurious extra second caused
           * by subtracting rounded absolute pressures. Other legs keep their
           * original ceiling, including the noninteger 0.333334bar probe. */
          time_for_ascend = nominal_three_metre_grid_leg ?
              (int)ceil(60.0 * 3.0 / (double)pDiveSettings->ascentRate_meterperminute) :
              (int)ceilf(pressure_difference * ascendrate_in_seconds_for_one_bar);''',1)
output_path.parent.mkdir(parents=True,exist_ok=True)
output_path.write_bytes(source.encode('latin1'))
