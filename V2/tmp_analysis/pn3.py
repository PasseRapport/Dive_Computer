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
        pin_list.append({'ref':s['ref'],'value':s['value'],'pin':p['number'],'name':p['name'],'pos':pos,'power':s['ref'].startswith('#PWR'),'dnp':s.get('dnp','no')})

for p in pin_list:
    parent.setdefault(p['pos'],p['pos']); connect_point(p['pos'])
for i in range(len(pin_list)):
    for j in range(i+1,len(pin_list)):
        if pin_list[i]['pos']==pin_list[j]['pos']:
            union(pin_list[i]['pos'],pin_list[j]['pos'])

net_names=defaultdict(set)
for l in labels:
    net_names[find(l['at'])].add(l['text'])
# power symbols name their net
for p in pin_list:
    if p['power'] and p['pin'] is not None:
        r=find(p['pos'])
        if p['value'] not in ('GND',) or True:
            net_names[r].add('PWR:'+p['value'])

def netname(r):
    t=net_names.get(r,set())
    if not t: return None
    txts=sorted(t)
    pwr=[x for x in txts if x.startswith('PWR:')]
    lbl=[x for x in txts if not x.startswith('PWR:')]
    s=[]
    if lbl: s.append('/'.join(lbl))
    if pwr: s.append(','.join(x[5:] for x in pwr))
    return ' | '.join(s)

# Full net dump for selected refs
SEL=['U2','U3','U6','U11']
print("=== NETS OF SELECTED PARTS ===")
for ref in SEL:
    pl=[p for p in pin_list if p['ref']==ref]
    if not pl: continue
    dnp = ' [DNP]' if pl[0]['dnp']=='yes' else ''
    print(f"\n--- {ref} ({pl[0]['value']}){dnp} ---")
    for p in pl:
        print(f"  pin {p['pin']:<4} net: {netname(find(p['pos']))}")

print("\n=== POWER RAILS (nets containing power syms) ===")
rails=defaultdict(set)
for p in pin_list:
    if p['power']:
        r=find(p['pos'])
        rails[netname(r) or f'(unlabeled @{p["pos"]})'].add(f"{p['ref']}/{p['value']}")
for k in sorted(rails):
    print(f"  {k}: {len(rails[k])} power-sym attachment(s)")
