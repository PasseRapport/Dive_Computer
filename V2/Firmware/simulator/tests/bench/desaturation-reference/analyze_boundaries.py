# SPDX-License-Identifier: GPL-3.0-or-later
import json, math, pathlib
from desaturation_reference import estimate, expression32
root=pathlib.Path(__file__).parent
surface=1.01325;b=.7902*(surface-.0627)
points=[]
for m in (1,59,60,61,754,1500):
    for delta in (-.001,0,.001):
        n2=[b]*16;n2[-1]=b+.05*b*math.exp((m+delta)*.6931/(635*1.42))
        points.append({'mathematical_target_minutes':m+delta,'binary64_algebra':estimate(n2,surface),'binary32_expression_diagnostic':expression32(n2,surface),'nitrogen_bar':n2})
(root/'boundary-analysis.json').write_text(json.dumps({'scope':'Separate analysis. Exact mathematical boundaries are sensitive to input/expression rounding; no tolerance is granted. Selected profile expectations remain frozen unchanged.','points':points},indent=2)+'\n')
for x in points:
    print(x['mathematical_target_minutes'],x['binary64_algebra']['unquantized_minutes'],x['binary64_algebra']['minutes'],x['binary32_expression_diagnostic']['minutes'])
