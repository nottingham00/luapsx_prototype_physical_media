# LuaPSX disc-to-Games handoff

`luapsx-handoff.elf` is the small payload intended to be the final item in an
existing BD-J autoloader chain.

It does four things only:

1. If `/mnt/disc/LUAPSX/luapsx-ps5.elf` exists, copy it to
   `/data/luapsx/luapsx-ps5.elf`.
2. If the running big app is Disc Player (`NPXS40140`), suspend and close it.
3. Launch the installed LuaPSX Games title (`LPSX00001`).
4. If title launch fails, navigate back to Home instead of leaving the user in
   Disc Player.

It does **not** contain a jailbreak, kernel exploit or BD-J native-code exploit.
It expects the console to already be in a homebrew environment where the Sony
service imports used by ps5-payload-sdk are available.

Build:

```sh
export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
make -C handoff -f Makefile.ps5
```
