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
                if isinstance(c, list) and c[0]=='at': at=(float(c[1]),float(c[2]),float(c[3]) if len(c)>3 else 0.0)
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
                nm = ln[1]
                pins=[]; walk_pins(ln,pins)
                lib_pins[nm]=pins

# find instance U1
for node in tree:
    if isinstance(node, list) and node and node[0]=='symbol' and isinstance(node[1], list):
        inst=None
        for k in node[1:]:
            if isinstance(k,list) and k[0]=='lib_id': inst=k[1]
        if not inst or 'LIS2MDL' not in inst: continue
        ref=None
        for k in node[1:]:
            if isinstance(k,list) and k[0]=='property' and k[1]=='Reference': ref=k[2]
        at=None;rot=0.0;mirror=''
        for k in node[1:]:
            if isinstance(k,list) and k[0]=='at': at=(float(k[1]),float(k[2]));rot=float(k[3]) if len(k)>3 else 0.0
            if isinstance(k,list) and k[0]=='mirror' and len(k)>1: mirror=k[1]
        print("U1 instance: at=",at,"rot=",rot,"mirror=",repr(mirror),"ref=",ref)
        for nm,pins in lib_pins.items():
            if 'LIS2MDL' in nm:
                print(" libsym",nm,"pins:")
                for p in pins:
                    print("   pin",p['number'],p['name'],"local",p['at'])

# Now which wires/labels exist near U1
print()
print("=== wires with endpoint near (172,100)-(210,140) ===")
for node in tree:
    if isinstance(node, list) and node and node[0]=='wire':
        pts=[]
        for c in node[1:]:
            if isinstance(c,list) and c[0]=='pts':
                for p in c[1:]:
                    if isinstance(p,list) and p[0]=='xy': pts.append((float(p[1]),float(p[2])))
        for (x,y) in pts:
            if 150<x<230 and 90<y<150:
                print(" wire",pts)
                break
