#!/usr/bin/env python3
from pathlib import Path
import re, sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else "../disc/PSX")
cues = sorted(root.glob("*.cue")) + sorted(root.glob("*.CUE"))
if not cues:
    raise SystemExit(f"No CUE found in {root}")

cue = cues[0]
print(f"CUE: {cue}")
for line in cue.read_text(errors="replace").splitlines():
    m = re.match(r'\s*FILE\s+(?:"([^"]+)"|(\S+))', line, re.I)
    if not m:
        continue
    name = m.group(1) or m.group(2)
    p = cue.parent / name
    if not p.is_file():
        raise SystemExit(f"Missing image file: {p}")
    size = p.stat().st_size
    if size % 2352:
        raise SystemExit(f"{p}: {size} bytes is not divisible by 2352")
    print(f"OK: {p.name} - {size} bytes - {size // 2352} raw sectors")
