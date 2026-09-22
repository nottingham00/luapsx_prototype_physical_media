# Emulator Integration

## Current interface

LuaPSX uses a small libretro frontend instead of coupling the PS5 code directly to one emulator's private internals.

The default core path is:

```text
/data/luapsx/cores/pcsx_rearmed_libretro.so
```

The content path is selected at runtime:

```text
/data/luapsx/PSX/GAME.CUE
```

or, when the copy step is disabled/fails:

```text
/mnt/disc/PSX/GAME.CUE
```

## Frontend responsibilities

`native/src/luapsx_libretro.c` owns:

- `dlopen` + required `retro_*` symbol resolution
- `retro_set_environment`
- system/save directory callbacks
- default legacy core-option parsing
- pixel format negotiation
- logging
- disk-control interface registration
- frame timing
- core start/stop lifecycle
- libretro SaveRAM load/periodic flush/final save to `<game>.srm`

`native/src/platform_sdl.c` owns:

- window/renderer/texture creation
- 15/16/32-bit PS1 framebuffer conversion
- 4:3/aspect-preserving output
- stereo S16 audio queueing
- SDL GameController polling
- analog sticks/triggers and digital buttons

## DualSense mapping

The SDL GameController mapping is exposed to libretro as a standard PS-style joypad:

```text
Cross      -> RETRO B
Circle     -> RETRO A
Square     -> RETRO Y
Triangle   -> RETRO X
D-pad      -> D-pad
L1/R1      -> L/R
L2/R2      -> L2/R2
L3/R3      -> L3/R3
Options    -> Start
Create     -> Select
Left/right sticks -> libretro analog axes
```

SDL's logical A/B/X/Y naming can vary by platform mapping. If the PS5 SDL backend exposes PlayStation labels differently, only `platform_sdl.c` needs adjustment; the emulator core remains unchanged.

## Core compatibility checklist

A candidate PS1 core should export at least:

```text
retro_api_version
retro_init
retro_deinit
retro_get_system_info
retro_get_system_av_info
retro_set_environment
retro_set_video_refresh
retro_set_audio_sample
retro_set_audio_sample_batch
retro_set_input_poll
retro_set_input_state
retro_set_controller_port_device
retro_load_game
retro_unload_game
retro_run
retro_reset
retro_get_memory_data
retro_get_memory_size
```

The host fake core under `native/tests/` verifies this contract.
