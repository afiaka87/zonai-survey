# Zonai Survey v0.1.1

Pressing **ZL + D-pad Up** starts a Death-Stranding-inspired scan/survey that labels
items in front of you by ~440m. 

## Install on an emulator

1. Open the game's mod or load directory for title `0100F2C0115B6000`.
2. Extract the archive there. It should produce
   `zonai-survey/exefs/` and `zonai-survey/romfs/`.
3. Enable **zonai-survey** in the game's add-ons list, then restart the game.

The release archive include every font and shader file needed at runtime. Do not
remove the `romfs/` directory even if another mod appears to carry similar files.

## Switch hardware

Switch hardward is currently untested. The standard layout is to copy `exefs/` and
`romfs/` under `atmosphere/contents/0100F2C0115B6000/`.

## Limitations

- This ONLY runs on version 1.2.1 of TotK.
- This is a "subsdk9" mod - it can and will conflict/break other similar mods when enabled at the same time. One solution to this is to compile the source here alongside your other mods, but you would need to respect various polling systems to make sure they work together.
- The far field becomes sparse as the game streams less collision at distance.
- FPS currently may take a dip depending on hardware and choice of emulator. In my testing this goes from a 0 fps hit to as much as 5 fps. It could be even more on (for instance) Swithc hardware).

## Source and license

Zonai Survey is GPL-2.0-only. The text renderer and part of the overlay hook are
derived from [totk-lotuskit](https://github.com/aquacluck/totk-lotuskit) by aquacluck
and its contributors. See [NOTICE](NOTICE) for full attribution.

The source repository omits the eight ROMFS runtime files. The ready-to-install mod
archive includes them.