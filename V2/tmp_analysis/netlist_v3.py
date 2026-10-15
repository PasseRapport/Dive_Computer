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
                for c in n[1:]:
                    pass
            pins.append({'name':name,'number':number,'at':at}); return
        for c in n:
            if isinstance(c, list): walk_pins(c, pins)

lib_pins = {}
for node in tree:
    if isinstance(node, list) and node and node[0]=='lib_symbols':
        for ln in node[1:]:
            if isinstance(ln, list) and ln[0]=='symbol':
                nm = ln[1]; pins=[]; walk_pins(ln,pins); lib_pins[nm]=pins

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
            if isinstance(k, list) and k[0]=='property' and k[1]=='MPN': d['mpn']=k[2]
            if isinstance(k, list) and k[0]=='property' and k[1]=='Footprint': d['fp']=k[2]
            if isinstance(k, list) and k[0]=='dnp': d['dnp']=k[1]
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

# endpoint set for empirical transform calibration
endpoints=set()
for pts in wires:
    for pt in pts: endpoints.add(pt)
for l in labels: endpoints.add(l['at'])

def transform_candidates(px, py, X, Y, rot, mirror):
    # base screen offset: lib Y-up -> screen Y-down
    bx, by = px, -py
    # mirror: (mirror x) mirrors about the X axis => negate y AFTER conversion? test all combos
    if mirror:  # try both interpretations; calibration will pick right
        pass
    rr = math.radians(rot)
    # visual CCW rotation on screen (Y-down): (x,y) -> (x cos + y sin, -x sin + y cos)
    c, s = math.cos(rr), math.sin(rr)
    rx = bx*c + by*s
    ry = -bx*s + by*c
    out = []
    for mx in (1,-1):
        for my in (1,-1):
            out.append((X+mx*rx, Y+my*ry))
    return out

# For each symbol pick transform variant that maximizes endpoint hits.
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
    # calibrate: which (mx,my) sign combo hits most endpoints
    best=(0,0); besthits=-1
    for mx in (1,-1):
        for my in (1,-1):
            hits=0
            for p in pins:
                cands = transform_candidates(p['at'][0], p['at'][1], s['at'][0], s['at'][1], s['rot'], s['mirror'])
                # index: candidates order is (mx,my) in (1,1),(1,-1),(-1,1),(-1,-1)
                idx = {(1,1):0,(1,-1):1,(-1,1):2,(-1,-1):3}[(mx,my)]
                pos = (round(cands[idx][0],3), round(cands[idx][1],3))
                if pos in endpoints: hits+=1
            if hits>besthits:
                besthits=hits; best=(mx,my)
    for p in pins:
        cands = transform_candidates(p['at'][0], p['at'][1], s['at'][0], s['at'][1], s['rot'], s['mirror'])
        idx = {(1,1):0,(1,-1):1,(-1,1):2,(-1,-1):3}[best]
        pos=(round(cands[idx][0],3), round(cands[idx][1],3))
        pin_list.append({'ref':s['ref'],'value':s['value'],'pin':p['number'],'name':p['name'],'pos':pos,
                         'power':s['ref'].startswith('#PWR'),'dnp':s.get('dnp','no'),'tr':best})
    if pins and besthits<=len(pins)//2:
        pass

# union-find
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
for p in pin_list:
    parent.setdefault(p['pos'],p['pos']); connect_point(p['pos'])
for i in range(len(pin_list)):
    for j in range(i+1,len(pin_list)):
        if pin_list[i]['pos']==pin_list[j]['pos']:
            union(pin_list[i]['pos'],pin_list[j]['pos'])

net_names=defaultdict(set)
for l in labels:
    net_names[find(l['at'])].add(l['text'])
for p in pin_list:
    if p['power'] and p['pin'] is not None:
        net_names[find(p['pos'])].add('PWR:'+p['value'])

def netname(r):
    t=net_names.get(r,set())
    if not t: return None
    txts=sorted(t)
    pwr=[x for x in txts if x.startswith('PWR:')]
    lbl=[x for x in txts if not x.startswith('PWR:')]
    out=[]
    if lbl: out.append('/'.join(lbl))
    if pwr: out.append(','.join(x[5:] for x in pwr))
    return ' | '.join(out)

import json
# full report
IC_REFS=['U1','U2','U3','U4','U5','U6','U8','U9','U10','U11','U12','LIS2DUX12','FPC1']
print("=== FULL PIN NETLIST (correct transform) ===")
for ref in IC_REFS:
    pl=[p for p in pin_list if p['ref']==ref]
    if not pl: continue
    d = ' [DNP]' if pl[0]['dnp']=='yes' else ''
    print(f"\n--- {ref} ({pl[0]['value']}){d} ---")
    def sk(p):
        try: return (0,float(p['pin']))
        except: return (1,str(p['pin']))
    for p in sorted(pl,key=sk):
        r=find(p['pos'])
        nc=' NC' if p['pos'] in no_connects else ''
        fl='' if netname(r) else ' FLOAT'
        print(f"  pin {p['pin']:<4} [{p['name']:<14}] {netname(r) if netname(r) else '-'}{nc}{fl}")

print("\n=== RESISTORS/NTC ===")
for ref in ['R1','R2','R8','R9','R10','R11','R12','R3','R4','R5','R6','R7','NTC']:
    pl=[p for p in pin_list if p['ref']==ref]
    if not pl: continue
    outs=[]
    for p in pl:
        outs.append(f"{p['pin']}:{netname(find(p['pos'])) or '-'}")
    print(f"  {ref} ({pl[0]['value']}){' [DNP]' if pl[0]['dnp']=='yes' else ''}: {', '.join(outs)}")

print("\n=== SYMBOL PROPERTIES (MPN/footprint) ===")
for uuid,s in symbols.items():
    if s['ref'].startswith('#PWR'): continue
    print(f"  {s['ref']}: value={s['value']!r} mpn={s.get('mpn','-')!r} fp={s.get('fp','-')!r} dnp={s.get('dnp','no')}")
