# LuaPSX BD-J Physical Disc Prototype

LuaPSX is a proof-of-concept for a PS5-readable Blu-ray that carries a PS1 `BIN/CUE` image, a BD-J launcher, and optionally a PS1 libretro emulator core.

The project intentionally does **not** contain a PS5 firmware exploit, sandbox escape, Sony BIOS, or copyrighted game content. The native ELF assumes you already have a legitimate homebrew/ELF execution environment on your own console.

## What is implemented now

The prototype is split into three layers:

1. **BD-J Xlet**
   - Locates the Blu-ray root with `bluray.vfs.root`.
   - Finds `PSX/*.CUE`.
   - Parses the CUE and verifies every referenced BIN.
   - Checks that raw BIN files are aligned to 2352-byte CD sectors.
   - Shows whether a `pcsx_rearmed_libretro.so` core is included on the disc.

2. **Native PS5 disc/cache layer**
   - Opens `/mnt/disc/PSX`.
   - Copies the PS1 image to `/data/luapsx/PSX` when enabled.
   - Falls back to direct Blu-ray streaming if copying fails.
   - Parses CUE sheets and exposes raw 2352-byte and Mode-1 2048-byte sector reads.
   - Supports multi-file/multi-track CUE sheets within the current parser limits.
   - Creates `/data/luapsx/{cores,system,saves}`.
   - Stages `LUAPSX/cores` and `LUAPSX/system` from the Blu-ray into `/data/luapsx`.

3. **Libretro emulator frontend**
   - Dynamically loads a libretro core with `dlopen()`.
   - Loads the copied or directly streamed `.cue` path.
   - Implements libretro environment callbacks, default core options, save/system directories, logging, pixel-format negotiation and shutdown.
   - Implements SDL2 video with aspect-preserving letterboxing.
   - Converts `0RGB1555`, `RGB565`, and `XRGB8888` core framebuffers to the SDL texture.
   - Implements stereo 16-bit audio queuing.
   - Implements joypad + analog input through SDL GameController, suitable for DualSense via the PS5 SDL port.
   - Loads and periodically flushes libretro SaveRAM as `/data/luapsx/saves/<game>.srm`, giving PCSX-ReARMed memory-card slot 0 persistent storage.
   - Includes a host-side fake libretro core test so the frontend can be validated without a PS5.

## Blu-ray layout

```text
BDMV/
  ... BD-J authored files ...

PSX/
  GAME.CUE
  GAME.BIN

LUAPSX/
  cores/
    pcsx_rearmed_libretro.so     <- optional PS5 build of PCSX-ReARMed
  system/
    ... optional user-supplied BIOS/system files ...
```

The resulting native runtime layout is:

```text
/data/luapsx/
  PSX/
    GAME.CUE
    GAME.BIN
  cores/
    pcsx_rearmed_libretro.so
  system/
  saves/
```

If `COPY_TO_DATA=0`, the PS1 game can instead remain at `/mnt/disc/PSX` while the core still loads from `/data/luapsx/cores`.

## Games-area integration

LuaPSX now includes an optional permanent PS5 Games tile (`LPSX00001`) and a
small `handoff/luapsx-handoff.elf` payload. When used as the final step of an
existing BD-J autoloader, the handoff stages the emulator ELF, closes Disc
Player and asks the system to launch the installed LuaPSX title. The title then
forwards the emulator payload to the already-running local ELF loader on port
9021. See `docs/games-handoff.md`.

The title package template lives at `title/package/LPSX00001/`. A real title
requires a firmware-compatible signed `libc.prx`, which is not distributed.

Host test for the title-to-elfldr bridge:

```bash
make -C title -f Makefile.host test
```

## Runtime flow

```text
Blu-ray inserted
      |
      v
BD-J Xlet validates PSX/GAME.CUE + BIN
      |
      v
Existing PS5 homebrew/ELF execution environment
      |
      v
LuaPSX native payload
      |
      +--> stage LUAPSX/cores + LUAPSX/system
      |
      +--> copy PSX/ -> /data/luapsx/PSX
      |       or fall back to /mnt/disc/PSX
      |
      +--> validate CUE/BIN and raw sectors
      |
      v
dlopen(pcsx_rearmed_libretro.so)
      |
      v
retro_load_game(GAME.CUE)
      |
      +--> SDL2 video
      +--> SDL2 audio
      +--> SDL GameController / DualSense
      +--> /data/luapsx/saves
      v
PS1 game loop
```

## Host tests

The low-level disc parser/cache tests:

```bash
cd native
make -f Makefile.host
../tools/make_test_image.py ../disc/PSX
./psxdisc-test ../disc/PSX/GAME.CUE
./cache-copy-test ../disc/PSX /tmp/luapsx-cache-test
```

The libretro frontend test builds a tiny fake core and runs six emulated frames through the same frontend callbacks:

```bash
cd native
make -f Makefile.frontend-host test
```

The current checked-in test completes with:

```text
core: LuaPSX Fake Core 0.1
AV: 320x240, 60 fps, 44100 Hz
6 video frames
4410 stereo audio frames
131072-byte GAME.srm save RAM file
```

## PS5 payload build

Requirements:

- `ps5-payload-sdk`
- The `ps5-payload-dev/SDL` PS5 SDL2 port installed into a prefix
- A PS5-built libretro PS1 core such as PCSX-ReARMed for actual game emulation

Example:

```bash
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
cd native
make -f Makefile.ps5 \
  SDL2_PREFIX=/opt/ps5-payload-sdk
```

If SDL2 is installed elsewhere:

```bash
make -f Makefile.ps5 \
  SDL2_CFLAGS="-I/path/to/SDL2/include/SDL2" \
  SDL2_LIBS="-L/path/to/SDL2/lib -lSDL2"
```

The output is:

```text
native/luapsx-ps5.elf
```

Configuration switches:

```text
COPY_TO_DATA=1       copy PS1 BIN/CUE to /data before emulation
COPY_TO_DATA=0       stream the PS1 image directly from /mnt/disc
RUN_EMULATOR=1       launch the libretro frontend when a core exists
RUN_EMULATOR=0       only validate and sector-test the disc image
CORE_PATH=...        override /data/luapsx/cores/pcsx_rearmed_libretro.so
```

## PCSX-ReARMed integration

PCSX-ReARMed is the first target because it already supports libretro, `.cue/.bin`, HLE BIOS operation, PS1 audio/video/input and x86-family non-ARM dynarec paths upstream.

LuaPSX does **not** redistribute PCSX-ReARMed. Build it from the upstream GPLv2 source for the PS5 environment, then place the resulting libretro shared library at:

```text
disc/LUAPSX/cores/pcsx_rearmed_libretro.so
```

On boot, LuaPSX copies that file to:

```text
/data/luapsx/cores/pcsx_rearmed_libretro.so
```

and feeds the selected CUE to `retro_load_game()`.

The remaining porting task is therefore narrower than before: produce a PS5-compatible build of the upstream PCSX-ReARMed libretro core and resolve any PS5-specific libc/dynarec assumptions it exposes. The BD copy path, content path, frontend loop, frame/audio callbacks and input layer are already present.

## BIOS

No Sony PlayStation BIOS is included. PCSX-ReARMed can use an HLE BIOS for many games. If you choose to use an original BIOS, provide your own legally dumped file in `disc/LUAPSX/system/`.

## Important limitation

This is not yet a one-click retail-style PS5 disc. Standard BD-J by itself does not start the native ELF. The project deliberately leaves the firmware-specific native-code handoff outside this repository. Once the native payload is running, the emulator flow is implemented up to the point where a PS5-built libretro PS1 core is required.

## Staging the PS5 runtime onto the Blu-ray tree

After building the emulator payload and optional Games handoff payload:

```sh
./tools/stage_ps5_runtime.sh native/luapsx-ps5.elf handoff/luapsx-handoff.elf
```

PowerShell:

```powershell
.\tools\stage_ps5_runtime.ps1 -EmulatorElf native\luapsx-ps5.elf -HandoffElf handoff\luapsx-handoff.elf
```

The permanent Games title remains installed on the console; only the per-disc
runtime, artwork, metadata and PS1 image need to be authored onto each Blu-ray.
