import re, sys
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

def parse(toks):
    it = iter(toks)
    def helper():
        t = next(it)
        if t != '(':
            return t
        lst = []
        while True:
            t2 = next(it)
            if t2 == ')': return lst
            if t2 == '(':
                it2 = iter([t2])
                # simpler: recursive using index; do manual stack
                # fallback: use index-based approach below
                raise Exception('nested')
    # Use a proper index-based parser
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
    return parse_expr()

def parse_all(toks):
    pos = 0
    def parse_expr():
        nonlocal pos
        if pos >= len(toks):
            raise StopIteration
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

# Build symbol pin geometry: embedded (symbol "REF" (pin ...)) units
# Top-level s-exprs: symbol, wire, label, global_label, junction, etc.
# 1) Collect symbol instances: (symbol <lib_id> (at x y rot) ... (property "Reference" "U1") ... (uuid ...)) with mirror
symbols = {}  # uuid -> dict
for node in tree:
    if isinstance(node, list) and node and node[0] == 'symbol':
        # KiCad 8: (symbol (lib_id "...") (at ...) ...); older: (symbol "lib:Id" ...)
        if isinstance(node[1], str):
            d = {'lib_id': node[1]}
            kids = node[2:]
        else:
            d = {'lib_id': None}
            kids = node[1:]
            for k in kids:
                if isinstance(k, list) and k[0] == 'lib_id':
                    d['lib_id'] = k[1]
                    break
        at = None
        rot = 0.0
        for k in kids:
            if isinstance(k, list) and k[0] == 'at':
                at = (float(k[1]), float(k[2]))
                rot = float(k[3]) if len(k) > 3 else 0.0
        for k in kids:
            if isinstance(k, list) and k[0] == 'property' and k[1] == 'Reference': d['ref'] = k[2]
            if isinstance(k, list) and k[0] == 'property' and k[1] == 'Value': d['value'] = k[2]
            if isinstance(k, list) and k[0] == 'uuid': d['uuid'] = k[1]
            if isinstance(k, list) and k[0] == 'mirror': d['mirror'] = k[1] if len(k)>1 else ''
        d['at'] = at; d['rot'] = rot
        # unit info
        for k in kids:
            if isinstance(k, list) and k[0] == 'unit': d['unit'] = k[1]
        if d.get('ref') and not d['ref'].startswith('#'):
            symbols[d['uuid']] = d

# 2) Embedded lib symbol pin defs: (symbol "U?..." ... actually embedded defs come as
# top-level (symbol "REF_x_y" (pin ...)) inside (lib_symbols). Extract pins per lib symbol name.
lib_symbols = {}
for node in tree:
    if isinstance(node, list) and node and node[0] == 'lib_symbols':
        for lnode in node[1:]:
            collect_pins(lnode, [], lib_symbols) if False else None
# simpler: recursively find all (pin ...) under (lib_symbols (libname (symbol "sub" (pin ...))))

def walk_pins(n, pins):
    if isinstance(n, list):
        if n and n[0] == 'pin' and len(n) >= 4:
            # (pin type style (at x y angle) (length L) ... (name "NAME" ...) (number "N" ...))
            at = None; name=None; number=None
            for c in n[1:]:
                if isinstance(c, list) and c[0]=='at': at = (float(c[1]), float(c[2]), float(c[3]) if len(c)>3 else 0.0)
                if isinstance(c, list) and c[0]=='name': name = c[1]
                if isinstance(c, list) and c[0]=='number': number = c[1]
            pins.append({'type': n[1], 'name': name, 'number': number, 'at': at})
            return
        for c in n:
            if isinstance(c, list): walk_pins(c, pins)

lib_symbol_pins = {}
def collect_lib(node):
    if isinstance(node, list):
        if node and node[0] == 'symbol' and len(node) > 1 and isinstance(node[1], str):
            pins = []
            walk_pins(node, pins)
            if pins: lib_symbol_pins[node[1]] = pins
        for c in node:
            if isinstance(c, list): collect_lib(c)

for node in tree:
    if isinstance(node, list) and node and node[0] == 'lib_symbols':
        collect_lib(node)

# Map: symbol instance uses lib_id "Lib:Name"; embedded defs are keyed by "Name" and "Name_unit_x_y" etc.
# 3) Wires and labels
wires = []
labels = []
for node in tree:
    if isinstance(node, list) and node:
        if node[0] == 'wire':
            pts = []
            for c in node[1:]:
                if isinstance(c, list) and c[0] == 'pts':
                    for p in c[1:]:
                        if isinstance(p, list) and p[0]=='xy': pts.append((float(p[1]), float(p[2])))
            wires.append(pts)
        elif node[0] == 'label':
            at = None
            for c in node[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            labels.append({'text': node[1], 'at': at})
        elif node[0] == 'global_label':
            at = None
            for c in node[1:]:
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]))
            labels.append({'text': 'G:'+node[1], 'at': at})

# union-find on wire endpoints
parent = {}
def find(p):
    parent.setdefault(p,p)
    while parent[p]!=p:
        parent[p]=parent[parent[p]]; p=parent[p]
    return p
def union(a,b):
    parent.setdefault(a,a); parent.setdefault(b,b)
    ra,rb = find(a), find(b)
    if ra!=rb: parent[ra]=rb

for pts in wires:
    for i in range(len(pts)-1):
        union(pts[i], pts[i+1])

# points on same wire net (any point of a wire belongs to union of its endpoints)
def net_of(pt):
    return find(pt)

# add junctions as union points? junctions merge wire-crossings already handled by endpoints.
# 4) label -> root
label_roots = {}
for l in labels:
    r = net_of(l['at'])
    label_roots.setdefault(r, set()).add(l['text'])

# 5) symbol pin absolute positions
def pin_abs(sym, pin):
    x, y = sym['at']
    rot = sym['rot']
    px, py, pang = pin['at']
    m = sym.get('mirror','')
    # apply mirror then rotation
    mx, my = px, py
    if 'x' in m: mx = -mx
    if 'y' in m: my = -my
    import math
    rr = math.radians(rot)
    ax = x + mx*math.cos(rr) - my*math.sin(rr)
    ay = y + mx*math.sin(rr) + my*math.cos(rr)
    return (round(ax,3), round(ay,3))

# attach each symbol pin to nearest net via union point
pin_nets = []
for uuid, s in symbols.items():
    libname = s['lib_id'].split(':')[-1]
    pins = lib_symbol_pins.get(libname)
    if not pins:
        # try sub-symbols with unit
        pins = []
        for k,v in lib_symbol_pins.items():
            if k.startswith(libname): pins.extend(v)
    if not pins:
        continue
    for p in pins:
        pos = pin_abs(s, p)
        pin_nets.append({'ref': s['ref'], 'value': s['value'], 'pin': p['number'], 'name': p['name'], 'pos': pos, 'net': find(pos) if pos in parent else None})

# report: for each net that has a label, list connected pins
net_label = {}
for l in labels:
    r = net_of(l['at'])
    net_label.setdefault(r, set()).add(l['text'])

print("=== SIGNALS ===")
for sig in sorted(set().union(*[set(v) for v in net_label.values()])):
    pass
by_signal = defaultdict(list)
for r, texts in net_label.items():
    for pn in pin_nets:
        if pn['net'] == r and pn['net'] is not None:
            for t in texts:
                by_signal[t].append(pn)

for sig in sorted(by_signal):
    pins = by_signal[sig]
    print(f"\n--- {sig} ---")
    for p in pins:
        print(f"  {p['ref']} ({p['value']}) pin {p['pin']} [{p['name']}]")

print("\n=== SYMBOL PIN COUNTS ===")
for uuid,s in symbols.items():
    libname = s['lib_id'].split(':')[-1]
    n = len(lib_symbol_pins.get(libname, []))
    if n==0:
        for k,v in lib_symbol_pins.items():
            if k.startswith(libname): n+=len(v)
    print(f"{s['ref']}: {s['value']} pins={n}")
