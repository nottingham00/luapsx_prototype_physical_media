#!/usr/bin/env sh
set -eu

if [ "$#" -ne 1 ]; then
  echo "usage: $0 /path/to/pcsx_rearmed_libretro.so" >&2
  exit 2
fi

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/disc/LUAPSX/cores"
cp "$1" "$ROOT/disc/LUAPSX/cores/pcsx_rearmed_libretro.so"
echo "Staged core: $ROOT/disc/LUAPSX/cores/pcsx_rearmed_libretro.so"
