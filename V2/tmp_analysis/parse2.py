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
        if t != '(':
            return t
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

# ---- collect lib symbol pins ----
def walk_pins(n, pins):
    if isinstance(n, list):
        if n and n[0] == 'pin' and len(n) >= 4:
            at = None; name=None; number=None; etype=None
            for c in n[1:]:
                if isinstance(c, list) and c[0]=='at': at = (float(c[1]), float(c[2]), float(c[3]) if len(c)>3 else 0.0)
                if isinstance(c, list) and c[0]=='name': name = c[1]
                if isinstance(c, list) and c[0]=='number': number = c[1]
                if isinstance(c, list) and c[0]=='electrical': etype = c[1] if len(c)>1 else (n[1] if len(n)>1 else None)
            if at is None:
                # pin (type) (name ...) style: name/number may be inline tokens
                name = name or (n[2] if len(n)>2 else None)
                number = number or (n[3] if len(n)>3 else None)
            pins.append({'type': etype or (n[1] if len(n)>1 else None), 'name': name, 'number': number, 'at': at})
            return
        for c in n:
            if isinstance(c, list): walk_pins(c, pins)

lib_symbol_pins = {}
def collect_lib(node):
    if isinstance(node, list):
        if node and node[0] == 'symbol' and len(node) > 1 and isinstance(node[1], str):
            pins = []
            walk_pins(node, pins)
            if pins: lib_symbol_pins.setdefault(node[1], []).extend(pins)
        for c in node:
            if isinstance(c, list): collect_lib(c)

for node in tree:
    if isinstance(node, list) and node and node[0] == 'lib_symbols':
        collect_lib(node)

# ---- collect symbol instances (incl. power symbols) ----
symbols = {}
for node in tree:
    if isinstance(node, list) and node and node[0] == 'symbol':
        if isinstance(node[1], str):
            d = {'lib_id': node[1]}; kids = node[2:]
        else:
            d = {'lib_id': None}; kids = node[1:]
            for k in kids:
                if isinstance(k, list) and k[0] == 'lib_id': d['lib_id'] = k[1]
        at = None; rot = 0.0; mirror = ''
        for k in kids:
            if isinstance(k, list) and k[0] == 'at':
                at = (float(k[1]), float(k[2])); rot = float(k[3]) if len(k)>3 else 0.0
            if isinstance(k, list) and k[0] == 'mirror' and len(k)>1: mirror = k[1]
            if isinstance(k, list) and k[0] == 'property' and k[1] == 'Reference': d['ref'] = k[2]
            if isinstance(k, list) and k[0] == 'property' and k[1] == 'Value': d['value'] = k[2]
            if isinstance(k, list) and k[0] == 'uuid': d['uuid'] = k[1]
        d['at'] = at; d['rot'] = rot; d['mirror'] = mirror
        if d.get('ref') is not None and d.get('uuid'):
            symbols[d['uuid']] = d

# ---- wires, labels, junctions ----
wires = []; labels = []; junctions = []; no_connects = []
for node in tree:
    if isinstance(node, list) and node:
        if node[0] == 'wire':
            pts = []
            for c in node[1:]:
                if isinstance(c, list) and c[0] == 'pts':
                    for p in c[1:]:
                        if isinstance(p, list) and p[0]=='xy': pts.append((float(p[1]), float(p[2])))
            if len(pts) >= 2: wires.append(pts)
        elif node[0] in ('label','global_label'):
            at = None
            for c in node[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            labels.append({'text': ('G:' if node[0]=='global_label' else '')+node[1], 'at': at})
        elif node[0] == 'junction':
            at = None
            for c in node[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            junctions.append(at)
        elif node[0] == 'no_connect':
            at = None
            for c in node[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            no_connects.append(at)

# ---- union-find with point-on-segment association ----
parent = {}
def find(p):
    parent.setdefault(p,p)
    while parent[p]!=p:
        parent[p]=parent[parent[p]]; p=parent[p]
    return p
def union(a,b):
    ra,rb = find(a), find(b)
    if ra!=rb: parent[ra]=rb

EPS = 0.011
def on_segment(p, a, b):
    (px,py),(ax,ay),(bx,by) = p,a,b
    if abs(ax-bx) < 1e-9:
        return abs(px-ax) <= EPS and min(ay,by)-EPS <= py <= max(ay,by)+EPS
    if abs(ay-by) < 1e-9:
        return abs(py-ay) <= EPS and min(ax,bx)-EPS <= px <= max(ax,bx)+EPS
    return False

segments = []
for pts in wires:
    for i in range(len(pts)-1):
        segments.append((pts[i], pts[i+1]))

def connect_point(p):
    """union p with the net of any segment containing it; return root or None"""
    parent.setdefault(p,p)
    hit = False
    for (a,b) in segments:
        if on_segment(p,a,b):
            union(p,a); hit = True
    return find(p) if hit else None

# union wire endpoint-to-endpoint / endpoint-on-segment
for pts in wires:
    for pt in pts:
        connect_point(pt)

# junctions
for j in junctions: connect_point(j)

# labels
for l in labels:
    parent.setdefault(l['at'], l['at'])
    connect_point(l['at'])

# ---- symbol pin absolute positions ----
def pin_abs(sym, pin):
    x, y = sym['at']; rot = sym['rot']; mirror = sym['mirror']
    px, py = pin['at'][0], pin['at'][1]
    # KiCad: mirror about the axis named: (mirror x) -> negate y; (mirror y) -> negate x
    if mirror == 'x': py = -py
    elif mirror == 'y': px = -px
    rr = math.radians(rot)
    ax = x + px*math.cos(rr) - py*math.sin(rr)
    ay = y + px*math.sin(rr) + py*math.cos(rr)
    return (round(ax,3), round(ay,3))

pin_list = []
for uuid, s in symbols.items():
    libname = s['lib_id'].split(':')[-1]
    pins = list(lib_symbol_pins.get(libname, []))
    for k,v in lib_symbol_pins.items():
        if k != libname and (k == libname + '_0_1' or k == libname + '_1_1' or (k.startswith(libname) and k[len(libname):].replace('_','').isdigit())):
            pins.extend(v)
    if not pins: continue
    for p in pins:
        pos = pin_abs(s, p)
        pin_list.append({'ref': s['ref'], 'value': s['value'], 'pin': p['number'], 'name': p['name'], 'pos': pos, 'is_power': s['ref'].startswith('#PWR')})

# connect pins
for p in pin_list:
    parent.setdefault(p['pos'], p['pos'])
    connect_point(p['pos'])

# pins that are exactly at another pin's position (direct pin-to-pin)
for i in range(len(pin_list)):
    for j in range(i+1, len(pin_list)):
        if pin_list[i]['pos'] == pin_list[j]['pos']:
            union(pin_list[i]['pos'], pin_list[j]['pos'])

# ---- build nets ----
net_pins = defaultdict(list)
for p in pin_list:
    r = find(p['pos'])
    net_pins[r].append(p)

net_labels = defaultdict(set)
for l in labels:
    r = find(l['at'])
    net_labels[r].add(l['text'])

# ---- report ----
by_signal = defaultdict(list)
unlabeled = defaultdict(list)
for r, texts in net_labels.items():
    for p in net_pins.get(r, []):
        for t in texts:
            by_signal[t].append(p)

print("=== SIGNALS (label -> connected pins) ===")
for sig in sorted(by_signal):
    print(f"\n--- {sig} ---")
    for p in by_signal[sig]:
        pw = ' [PWR]' if p['is_power'] else ''
        print(f"  {p['ref']} ({p['value']}) pin {p['pin']} [{p['name']}]{pw}")

# For the MCU: list all its pins and their nets
print("\n=== nRF52833 (U2) PIN NETS ===")
for p in sorted(pin_list, key=lambda x: (x['ref']=='U2', 0)):
    if p['ref'] == 'U2':
        r = find(p['pos'])
        texts = net_labels.get(r, set())
        nc = ' [NC]' if p['pos'] in no_connects else ''
        print(f"  pin {p['pin']} [{p['name']}] net={'/'.join(sorted(texts)) if texts else '-'}{nc}")

print("\n=== FPC1 (screen conn) PIN NETS ===")
for p in pin_list:
    if p['ref'] == 'FPC1':
        r = find(p['pos'])
        texts = net_labels.get(r, set())
        print(f"  pin {p['pin']} net={'/'.join(sorted(texts)) if texts else '-'}")

print("\n=== ALL SYMBOLS ===")
for uuid,s in symbols.items():
    if not s['ref'].startswith('#PWR'):
        print(f"{s['ref']}: {s['value']} @ {s['at']} rot={s['rot']} mirror='{s['mirror']}'")
