import sys
sys.path.insert(0, '.')
exec(open('parse_sch.py').read().split('print("=== SIGNALS ===")')[0])
print("TREE:", len(tree), type(tree[0]))
from collections import Counter
print(Counter(n[0] for n in tree if isinstance(n, list)).most_common(20))
print("symbols:", len(symbols))
print("lib_symbol_pins keys:", list(lib_symbol_pins.keys()))
print("labels:", len(labels), [l['text'] for l in labels[:10]])
print("wires:", len(wires))
