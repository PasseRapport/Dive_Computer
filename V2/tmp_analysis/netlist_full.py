import math
from collections import defaultdict

path = '/home/Bilal/Documents/GitHub/Dive_Computer/V2/Hardware/DiveComputerV2/DiveComputerV2.kicad_sch'
text = open(path).read()

def tokenize(s):
    toks = []
    i = 0
    while i < len(s):
        c = s[i]
        if c in '()': toks.append(c); i += 1
        elif c == '"':
            j = i+1
            while j < len(s) and s[j] != '"': j += 1
            toks.append(s[i+1:j]); i = j+1
        elif c.isspace(): i += 1
        else:
            j = i
            while j < len(s) and not s[j].isspace() and s[j] not in '()"': j += 1
            toks.append(s[i:j]); i = j
    return toks

def parse_all(toks):
    pos = 0
    def parse_expr():
        nonlocal pos
        t = toks[pos]; pos += 1
        if t != '(': return t
        out = []
        while toks[pos] != ')':
            out.append(parse_expr())
        pos += 1
        return out
    exprs = []
    while pos < len(toks):
        exprs.append(parse_expr())
    return exprs

tree = parse_all(tokenize(text))
if len(tree) == 1 and isinstance(tree[0], list) and tree[0] and tree[0][0] == 'kicad_sch':
    tree = tree[0][1:]

def walk_pins(n, pins):
    if isinstance(n, list):
        if n and n[0] == 'pin' and len(n) >= 4:
            at=None; name=None; number=None
            for c in n[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
                if isinstance(c, list) and c[0]=='name': name=c[1]
                if isinstance(c, list) and c[0]=='number': number=c[1]
            if at is None:
                # pins with at inside (name..) form
                for c in n[1:]:
                    pass
            pins.append({'name':name,'number':number,'at':at})
            return
        for c in n:
            if isinstance(c, list): walk_pins(c, pins)

lib_pins = {}
for node in tree:
    if isinstance(node, list) and node and node[0]=='lib_symbols':
        for ln in node[1:]:
            if isinstance(ln, list) and ln[0]=='symbol':
                nm = ln[1]
                pins=[]; walk_pins(ln,pins)
                lib_pins[nm]=pins

# unit sub-symbols: key "libname_unit_x" (full name "Root_unit_x")
symbols = {}
for node in tree:
    if isinstance(node, list) and node and node[0] == 'symbol':
        if isinstance(node[1], str):
            d = {'lib_id': node[1]}; kids = node[2:]
        else:
            d = {'lib_id': None}; kids = node[1:]
            for k in kids:
                if isinstance(k, list) and k[0] == 'lib_id': d['lib_id'] = k[1]
        at=None;rot=0.0;mirror=''
        for k in kids:
            if isinstance(k, list) and k[0]=='at':
                at=(float(k[1]),float(k[2])); rot=float(k[3]) if len(k)>3 else 0.0
            if isinstance(k, list) and k[0]=='mirror' and len(k)>1: mirror=k[1]
            if isinstance(k, list) and k[0]=='property' and k[1]=='Reference': d['ref']=k[2]
            if isinstance(k, list) and k[0]=='property' and k[1]=='Value': d['value']=k[2]
            if isinstance(k, list) and k[0]=='uuid': d['uuid']=k[1]
        d.update({'at':at,'rot':rot,'mirror':mirror})
        if d.get('ref') is not None and d.get('uuid'):
            symbols[d['uuid']] = d

wires=[];labels=[];junctions=[];no_connects=[]
for node in tree:
    if isinstance(node, list) and node:
        if node[0]=='wire':
            pts=[]
            for c in node[1:]:
                if isinstance(c,list) and c[0]=='pts':
                    for p in c[1:]:
                        if isinstance(p,list) and p[0]=='xy': pts.append((float(p[1]),float(p[2])))
            if len(pts)>=2: wires.append(pts)
        elif node[0] in ('label','global_label'):
            at=None
            for c in node[1:]:
                if isinstance(c,list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            labels.append({'text':('G:' if node[0]=='global_label' else '')+node[1],'at':at})
        elif node[0]=='junction':
            at=None
            for c in node[1:]:
                if isinstance(c,list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            junctions.append(at)
        elif node[0]=='no_connect':
            at=None
            for c in node[1:]:
                if isinstance(c,list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            no_connects.append(at)

parent={}
def find(p):
    parent.setdefault(p,p)
    while parent[p]!=p:
        parent[p]=parent[parent[p]]; p=parent[p]
    return p
def union(a,b):
    ra,rb=find(a),find(b)
    if ra!=rb: parent[ra]=rb

EPS=0.011
def on_seg(p,a,b):
    (px,py),(ax,ay),(bx,by)=p,a,b
    if abs(ax-bx)<1e-9: return abs(px-ax)<=EPS and min(ay,by)-EPS<=py<=max(ay,by)+EPS
    if abs(ay-by)<1e-9: return abs(py-ay)<=EPS and min(ax,bx)-EPS<=px<=max(ax,bx)+EPS
    return False
segments=[(pts[i],pts[i+1]) for pts in wires for i in range(len(pts)-1)]
def connect_point(p):
    parent.setdefault(p,p)
    hit=False
    for (a,b) in segments:
        if on_seg(p,a,b): union(p,a); hit=True
    return find(p) if hit else None

for pts in wires:
    for pt in pts: connect_point(pt)
for j in junctions: connect_point(j)
for l in labels:
    parent.setdefault(l['at'],l['at']); connect_point(l['at'])

def pin_abs(sym,pin):
    x,y=sym['at']; rot=sym['rot']; mirror=sym['mirror']
    px,py=pin['at']
    if mirror=='x': py=-py
    elif mirror=='y': px=-px
    rr=math.radians(rot)
    return (round(x+px*math.cos(rr)-py*math.sin(rr),3), round(y+px*math.sin(rr)+py*math.cos(rr),3))

pin_list=[]
for uuid,s in symbols.items():
    libname=s['lib_id'].split(':')[-1]
    root_key=s['lib_id']
    pins=list(lib_pins.get(root_key,[]))
    if not pins: pins=list(lib_pins.get(libname,[]))
    if not pins:
        for k,v in lib_pins.items():
            if k.startswith(libname+'_'): pins.extend(v)
    if not pins: continue
    for p in pins:
        pos=pin_abs(s,p)
        pin_list.append({'ref':s['ref'],'value':s['value'],'pin':p['number'],'name':p['name'],'pos':pos,'power':s['ref'].startswith('#PWR')})

for p in pin_list:
    parent.setdefault(p['pos'],p['pos']); connect_point(p['pos'])
for i in range(len(pin_list)):
    for j in range(i+1,len(pin_list)):
        if pin_list[i]['pos']==pin_list[j]['pos']:
            union(pin_list[i]['pos'],pin_list[j]['pos'])

net_labels=defaultdict(set)
for l in labels:
    net_labels[find(l['at'])].add(l['text'])

def net_name(r):
    t=net_labels.get(r,set())
    return '/'.join(sorted(t)) if t else None

IC_REFS=['U1','U2','U3','U4','U5','U6','U8','U9','U10','U11','U12','LIS2DUX12','FPC1']
print("=== FULL PIN NETLIST PER IC ===")
for ref in IC_REFS:
    print(f"\n--- {ref} ---")
    pl=[p for p in pin_list if p['ref']==ref]
    def sortkey(p):
        try: return (0,float(p['pin']))
        except: return (1,str(p['pin']))
    for p in sorted(pl,key=sortkey):
        r=find(p['pos'])
        nn=net_name(r)
        nc=' NC' if p['pos'] in no_connects else ''
        fl=' FLOAT' if nn is None else ''
        print(f"  pin {p['pin']:<4} [{p['name']:<14}] net: {nn}{nc}{fl}")

print("\n=== RESISTORS OF INTEREST ===")
for ref in ['R1','R2','R8','R9','R10','R11','R12','R7','R3','R5','R6','R4']:
    pl=[p for p in pin_list if p['ref']==ref]
    outs=[]
    for p in pl:
        outs.append(f"pin{p['pin']}:{net_name(find(p['pos']))}")
    print(f"  {ref} ({pl[0]['value'] if pl else ''}): {', '.join(outs)}")

print("\n=== ALL LABELS WITH NOTHING CONNECTED ===")
for l in labels:
    r=find(l['at'])
    pins_on=[p for p in pin_list if find(p['pos'])==r and not p['power']]
    if not pins_on:
        print(f"  label '{l['text']}' at {l['at']}: no non-power pin on net (power syms: {[p['ref'] for p in pin_list if find(p['pos'])==r and p['power']]})")
