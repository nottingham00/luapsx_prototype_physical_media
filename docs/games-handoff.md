# Media-to-Games handoff

LuaPSX now has an optional two-stage shell integration so inserting the BD can
leave Disc Player and enter the permanent LuaPSX Games title.

```text
insert LuaPSX BD
      |
      v
BD-J / existing autoloader chain
      |
      v
luapsx-handoff.elf
      |-- stage /mnt/disc/LUAPSX/luapsx-ps5.elf -> /data/luapsx/
      |-- close NPXS40140 (Disc Player)
      |-- launch LPSX00001
      v
LuaPSX Games tile
      |
      |-- connect to localhost:9021
      |-- send staged luapsx-ps5.elf
      v
LuaPSX emulator payload
      |
      |-- read artwork/meta from /mnt/disc/PSX
      |-- read/copy GAME.CUE + GAME.BIN
      v
PCSX-ReARMed/libretro
```

The shell title is intentionally stable and permanent. Swapping discs does not
register/unregister per-game PS5 titles, so the home screen is not polluted with
stale entries.

## Failure behavior

The handoff first uses `sceSystemServiceLaunchApp()` to start `LPSX00001`. That
works in several existing PS5 homebrew launch paths, but homebrew/mounted-title
launch restrictions vary by environment. If the launch service rejects the
request, the handoff calls `sceShellCoreUtilNavigateToGoHome()` so the user is
at Home/Games instead of being left in Disc Player. The LuaPSX tile can then be
selected manually.

The project does not inject into ShellUI to bypass caller-context checks. That
keeps this integration simpler and avoids coupling LuaPSX to a firmware-specific
ShellUI RPC implementation.
