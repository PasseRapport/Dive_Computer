import sys
sys.argv=['x']
exec(open('/home/Bilal/Documents/GitHub/Dive_Computer/V2/tmp_analysis/netlist_v3.py').read().split('print("=== FULL')[0])

# group nets by power-symbol value
rail_nets = defaultdict(list)  # value -> list of roots
for p in pin_list:
    if p['power']:
        r=find(p['pos'])
        rail_nets[p['value']].append(r)

for val in sorted(rail_nets):
    roots = sorted(set(rail_nets[val]))
    print(f"\n######## power value {val!r}: {len(roots)} distinct net(s) ########")
    for r in roots:
        members=[p for p in pin_list if find(p['pos'])==r and not p['power']]
        lbls=[l['text'] for l in labels if find(l['at'])==r]
        print(f"  net root {r}  labels={lbls}")
        for m in members:
            print(f"    {m['ref']} ({m['value']}) pin {m['pin']} [{m['name']}]")
