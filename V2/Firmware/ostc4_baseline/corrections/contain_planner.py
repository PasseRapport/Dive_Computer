# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate containment accessors without changing planner arithmetic.
Pinned source remains unchanged. Lift the six backup statics to file scope so
both branches of backup/restore are captured along with every planner global.
"""
from pathlib import Path
import hashlib
import json
import re
import sys
path = Path(sys.argv[1])
original = path.read_bytes()
manifest = json.loads((path.parents[3] / "upstream-manifest.json").read_text())
if hashlib.sha256(original).hexdigest() != manifest["files"]["Discovery/Src/buehlmann.c"]:
    raise SystemExit("refusing to contain changed upstream planner")
source = original.decode("latin1")
start = source.index("static void buehlmann_backup_and_restore(_Bool backup_restore_otherwise)\r\n{")
end = source.index("float buehlmann_get_gCNS", start)
body = source[start:end]
fields = [("pressure", "float", ""), ("gas_id", "float", ""),
          ("tissue_nitrogen_bar", "float", "[16]"), ("tissue_helium_bar", "float", "[16]"),
          ("gf_value", "float", ""), ("cns", "float", "")]
declarations = ""
for name, kind, array in fields:
    declaration = f"static {kind} {name}{array};"
    if body.count(declaration) != 1:
        raise SystemExit("unexpected backup storage: " + declaration)
    body = body.replace(declaration, "")
    body = re.sub(r"\b" + name + r"\b", "saved_" + name, body)
    declarations += f"static {kind} saved_{name}{array};\n"
source = source[:start] + declarations + body + source[end:]
pairs = [("surface", "gSurface_pressure_bar"), ("pressure", "gPressure"),
         ("nitrogen", "gTissue_nitrogen_bar"), ("helium", "gTissue_helium_bar"),
         ("gf", "gGF_value"), ("cns", "gCNS"), ("gf_depth", "gGF_low_depth_bar"),
         ("gas_id", "gGas_id"), ("stop_depth", "gStop.depth"), ("stop_id", "gStop.id"),
         ("backup_pressure", "saved_pressure"), ("backup_gas_id", "saved_gas_id"),
         ("backup_nitrogen", "saved_tissue_nitrogen_bar"),
         ("backup_helium", "saved_tissue_helium_bar"),
         ("backup_gf", "saved_gf_value"), ("backup_cns", "saved_cns")]
source += '\n#include "ostc4_scratch.h"\n'
for restore in (False, True):
    source += f"void ostc4_scratch_{'restore' if restore else 'save'}({'const ' if restore else ''}ostc4_scratch *s) {{\n"
    for field, name in pairs:
        a, b = (name, "s->"+field) if restore else ("s->"+field, name)
        source += f"    memcpy(&{a}, &{b}, sizeof({a}));\n"
    source += "}\n"
output = Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(source.encode("latin1"))
