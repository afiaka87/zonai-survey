# Zonai Survey v0.1.2

Pressing **ZL + D-pad Up** starts a Death-Stranding-inspired scan/survey that labels
items in front of you by ~440m. 

## Install on an emulator

1. Open the game's mod or load directory for title `0100F2C0115B6000`.
2. Extract `zonai-survey-v0.1.2.zip` there. It should produce
   `zonai-survey/exefs/` and `zonai-survey/romfs/`.
3. Enable **zonai-survey** in the game's add-ons list, then restart the game.

The release archive includes every font and shader file needed at runtime. Do not
remove the `romfs/` directory even if another mod appears to carry similar files.

## Switch hardware

Extract `zonai-survey-switch-v0.1.2.zip` to the root of the SD card. Merge the
directories; do not replace the existing `atmosphere/` directory. The archive places:

- executable files under `atmosphere/contents/0100F2C0115B6000/exefs/`;
- all eight font and shader files under `zonai-survey/Lib/sead/`.

Both archives contain the same executable. There is no separate asset download or
manual preparation step. Keep the complete `zonai-survey/` directory on the SD card.
If upgrading from the old Switch layout, remove only Survey's old
`atmosphere/contents/0100F2C0115B6000/romfs/Lib/sead/` files, leaving other mods intact.

The startup changes address suspected hardware failure paths, but have not been
tested on a physical Switch. Eden and Citron regressions passed for both asset
routes. This is not yet a confirmed fix for the reported hardware crash.

## Changes in v0.1.2

- Check the supported game code and required assets before starting Survey.
- Fall back to the SD-card asset directory when the content directory is incomplete.
- Disable Survey when startup checks fail instead of continuing into initialization.
- Avoid unresolved color defaults when drawing scan lines.
- Restore the callback registration omitted from the previous public source tree.

## Limitations

- This ONLY runs on version 1.2.1 of TotK.
- This is a "subsdk9" mod - it can and will conflict/break other similar mods when enabled at the same time. One solution to this is to compile the source here alongside your other mods, but you would need to respect various polling systems to make sure they work together.
- The far field becomes sparse as the game streams less collision at distance.
- FPS currently may take a dip depending on hardware and choice of emulator. In my testing this goes from a 0 fps hit to as much as 5 fps. It could be even more on Switch hardware.

## Source and license

Zonai Survey is GPL-2.0-only. The text renderer and part of the overlay hook are
derived from [totk-lotuskit](https://github.com/aquacluck/totk-lotuskit) by aquacluck
and its contributors. See [NOTICE](NOTICE) for full attribution.

The source repository omits the eight ROMFS runtime files. The ready-to-install mod
archive includes them.
