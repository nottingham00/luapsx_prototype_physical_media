#!/usr/bin/env python3
from pathlib import Path
import sys

SECTOR = 2352
DATA = 2048

out = Path(sys.argv[1] if len(sys.argv) > 1 else "../disc/PSX")
out.mkdir(parents=True, exist_ok=True)
bin_path = out / "GAME.BIN"
cue_path = out / "GAME.CUE"

with bin_path.open("wb") as f:
    for lba in range(32):
        sec = bytearray(SECTOR)
        sec[:12] = bytes([0x00] + [0xff] * 10 + [0x00])
        sec[12:15] = bytes([0, 2, lba % 75])
        sec[15] = 0x01
        payload = (f"LuaPSX test sector {lba:04d}\n".encode("ascii"))
        sec[16:16+len(payload)] = payload
        for i in range(16 + len(payload), 16 + DATA):
            sec[i] = (lba + i) & 0xff
        f.write(sec)

cue_path.write_text('FILE "GAME.BIN" BINARY\n  TRACK 01 MODE1/2352\n    INDEX 01 00:00:00\n', encoding="ascii")
print(cue_path)
print(bin_path)
