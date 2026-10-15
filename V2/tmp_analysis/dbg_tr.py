import sys
sys.argv=['x']
exec(open('/home/Bilal/Documents/GitHub/Dive_Computer/V2/tmp_analysis/netlist_v3.py').read().split('print("=== FULL')[0])
# chosen transforms per IC
from collections import Counter
tr = {}
for p in pin_list:
    if p['ref'] in ('U2','U1','FPC1','U12','U5','LIS2DUX12','U9','U4','U10','U8'):
        tr.setdefault(p['ref'], p['tr'])
print(tr)
for ref in ['U2']:
    for p in pin_list:
        if p['ref']==ref and p['pin'] in ('26','38','39','8','16','19'):
            print(ref, 'pin', p['pin'], p['name'], 'pos', p['pos'], 'net', netname(find(p['pos'])))

print("=== net members around DEC4/DEC6 ===")
targets = [(191.77,90.17),(146.05,60.96)]
for t in targets:
    r=find(t)
    print("pin at",t,"root",r)
    for l in labels:
        if find(l['at'])==r: print("   label:",l['text'],l['at'])
    for p in pin_list:
        if find(p['pos'])==r:
            print("   pin:",p['ref'],p['value'],p['pin'],p['name'],p['pos'],'POWER' if p['power'] else '')
