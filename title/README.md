# LuaPSX permanent Games title

This directory implements the small permanent **LuaPSX** tile that lives in the
PS5 Games area as title ID `LPSX00001`.

The current development architecture deliberately keeps this title small:

```text
BD-J/autoloader
      |
      v
luapsx-handoff.elf
      | closes Disc Player
      | launches LPSX00001
      v
LuaPSX Games title
      |
      | sends /data/luapsx/luapsx-ps5.elf to localhost:9021
      v
existing elfldr
      v
LuaPSX SDL/libretro emulator payload
      v
/mnt/disc/PSX/GAME.CUE
```

This means the user returns to the Games side of the PS5 shell instead of
remaining in Media/Disc Player, while the emulator can keep using the existing
payload-SDK build during development.

## Host regression test

```sh
make -C title -f Makefile.host test
```

It starts a tiny local TCP receiver and verifies the title's elfldr client sends
an ELF byte-for-byte.

## Build the real title

Requirements:

- ps5-payload-sdk
- ps5link-sdk
- SharpProspero signing tool
- a firmware-compatible signed `libc.prx` supplied by the console owner

```sh
make -C title -f Makefile.ps5link \
  PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk \
  PS5LINK_SDK=$HOME/ps5link-sdk \
  SHARPPROSPERO=$HOME/SharpProspero
```

Then place the required signed `libc.prx` at:

```text
title/package/LPSX00001/sce_module/libc.prx
```

Install the complete `LPSX00001` directory through a compatible folder-title
registrar such as ShadowMountPlus. Do not install `eboot.bin` by itself.

## Important runtime dependency

The tile-to-emulator bridge assumes an ELF loader is already listening on
`127.0.0.1:9021`, which is normally true immediately after the BD-J autoloader
chain. If LuaPSX is opened manually without elfldr running, the title exits with
an error instead of attempting an exploit.
