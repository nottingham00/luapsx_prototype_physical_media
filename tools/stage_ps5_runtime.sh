#!/usr/bin/env sh
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
  echo "usage: $0 /path/to/luapsx-ps5.elf [/path/to/luapsx-handoff.elf]" >&2
  exit 2
fi
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/disc/LUAPSX" "$ROOT/disc/LUAPSX/autoload"
cp "$1" "$ROOT/disc/LUAPSX/luapsx-ps5.elf"
echo "staged emulator: disc/LUAPSX/luapsx-ps5.elf"
if [ "$#" -eq 2 ]; then
  cp "$2" "$ROOT/disc/LUAPSX/autoload/luapsx-handoff.elf"
  echo "staged handoff: disc/LUAPSX/autoload/luapsx-handoff.elf"
fi
