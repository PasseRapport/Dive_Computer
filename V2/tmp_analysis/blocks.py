import sys
sys.argv=['x']
exec(open('/home/Bilal/Documents/GitHub/Dive_Computer/V2/tmp_analysis/netlist_v3.py').read().split('print("=== FULL')[0])

def dump(ref):
    pl=[p for p in pin_list if p['ref']==ref]
    if not pl:
        print(f"--- {ref}: NOT FOUND ---"); return
    print(f"--- {ref} ({pl[0]['value']}) dnp={pl[0]['dnp']} ---")
    for p in pl:
        r=find(p['pos'])
        # all members
        mem=[m for m in pin_list if find(m['pos'])==r and m is not p]
        names=set()
        for m in mem:
            names.add(f"{m['ref']}.{m['pin']}[{m['name']}]{'*' if m['power'] else ''}")
        lbl=[l['text'] for l in labels if find(l['at'])==r]
        print(f"  pin {p['pin']:<3} [{p['name']:<8}] @ {p['pos']} net={netname(r)} || {', '.join(sorted(names)) or '-'} labels={lbl}")

for ref in ['X1','X2','ANT1','L1','L2','L3','L4','L5','C6','HAPTIC_MOTOR','BATTERY','J1','J2','NTC','R3','R4','R5','R6','R7','R10']:
    dump(ref)

print("\n=== labels list (all) ===")
seen=set()
for l in labels:
    key=(l['text'],)
    if key not in seen:
        seen.add(key)
        # count occurrences
print()
from collections import Counter
c=Counter(l['text'] for l in labels)
for t,n in sorted(c.items()):
    print(f"  {t}: {n}x")
