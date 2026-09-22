# Runtime flow

```text
Blu-ray inserted
      |
      v
BD-J Xlet
      |
      +--> bluray.vfs.root/PSX/GAME.CUE
      +--> validate CUE + BIN references
      |
      v
Existing homebrew/ELF loader
      |
      v
luapsx-disc-ps5.elf
      |
      +--> /mnt/disc/PSX/GAME.CUE
      +--> parse track layout
      +--> fopen/lseek/read GAME.BIN
      |
      v
psx_disc_read_raw2352(lba)
      |
      v
PS1 emulator CD-ROM callback
      |
      +--> CPU/GTE
      +--> GPU -> PS5 video backend
      +--> SPU -> PS5 audio backend
      +--> pad -> DualSense backend
```

## Why stream instead of copy?

The PS1 image stays on the Blu-ray. The emulator performs random 2352-byte sector reads from the mounted disc. This avoids needing hundreds of megabytes of BD-J persistent storage and keeps the physical disc meaningful during play.

A cache can be added later if optical seek latency becomes a problem.
