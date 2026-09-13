# Zonai Survey v0.1.3

Pressing **ZL + D-pad Up** starts a Death-Stranding-inspired scan/survey that labels
items in front of you by ~440m. 

## Install on an emulator

1. Open the game's mod or load directory for title `0100F2C0115B6000`.
2. Extract `zonai-survey-v0.1.3.zip` there. It should produce
   `zonai-survey/exefs/` and `zonai-survey/romfs/`.
3. Enable **zonai-survey** in the game's add-ons list, then restart the game.

The release archive includes every font and shader file needed at runtime. Do not
remove the `romfs/` directory even if another mod appears to carry similar files.

## Switch hardware

Extract `zonai-survey-switch-v0.1.3.zip` to the root of the SD card. Merge the
directories; do not replace the existing `atmosphere/` directory. The archive places:

- executable files under `atmosphere/contents/0100F2C0115B6000/exefs/`;
- all eight font and shader files under `zonai-survey/Lib/sead/`.

Both archives contain the same executable. There is no separate asset download or
manual preparation step. Keep the complete `zonai-survey/` directory on the SD card.
If upgrading from the old Switch layout, remove only Survey's old
`atmosphere/contents/0100F2C0115B6000/romfs/Lib/sead/` files, leaving other mods intact.

This renderer and the shared performance improvements have been tested on a
physical Switch and Eden, including repeated scans without reported crashes.
Dense grass may still cause a brief drop of approximately 3-4 fps on Switch.

## Changes in v0.1.3

- Replaced raycast-based scan geometry with depth-based surface lines and contours.
- Both the item scan and surface effect cover 100 degrees in front of Link.
- Ground lines reveal outward, remain in place and fade progressively over about 3.2 seconds.
- Grass tips receive a subdued blue accent; taller faces receive warmer contours and grids.
- Reduced Switch rendering cost without reducing the accepted grass-tip detail.
- Reduced module and drawing-buffer memory; collectible data stays lossless.
- Removed the end-of-survey repeat-button reminder.

## Limitations

- This ONLY runs on version 1.2.1 of TotK.
- This is a "subsdk9" mod - it can and will conflict/break other similar mods when enabled at the same time. One solution to this is to compile the source here alongside your other mods, but you would need to respect various polling systems to make sure they work together.
- Surface lines use the visible scene depth; they cannot recover hidden or off-screen surfaces.
- Low surfaces stay blue by height, not by material, so some low wall bases also stay blue.
- Dense grass may briefly cost about 3-4 fps on Switch. A locked 30 fps is not guaranteed.

## Source and license

Zonai Survey is GPL-2.0-only. The text renderer and part of the overlay hook are
derived from [totk-lotuskit](https://github.com/aquacluck/totk-lotuskit) by aquacluck
and its contributors. See [NOTICE](NOTICE) for full attribution.

The source repository omits the eight ROMFS runtime files. The ready-to-install mod
archive includes them.
