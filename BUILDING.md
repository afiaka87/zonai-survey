# Building Zonai Survey

The source is GPL-2.0-only. exlaunch, compatibility headers, the TotK 1.2.1 relocation
symbols, devkitA64, and Nintendo Switch packaging tools stay as external dependencies.
They are not vendored into this repository.

## Required inputs

- devkitPro with devkitA64 and the Switch packaging tools
- CMake 3.25 or newer and Ninja
- exlaunch at commit `f698816d6e198afb0029ad5c07d55e7017a620fe`
- compatible `nnheaders`, `agl`, and `sead` headers
- the `TOTK_121` relocation include and linker symbol files

Clone exlaunch, check out the pinned commit, initialize its submodules, then apply the
three patches under `patches/exlaunch/` with
`git apply --ignore-space-change -p3`. The build ignores exlaunch's example
`source/program/` directory and uses the configuration under this repository's `src/lib/`.

Configure from the repository root. Replace the example paths with your own local
paths:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE=cmake/TotkToolchain.cmake `
  -DDEVKITPRO_ROOT=/path/to/devkitpro `
  -DEXLAUNCH_ROOT=/path/to/exlaunch `
  -DTOTK_SDK_ROOT=/path/to/totk-sdk `
  -DTOTK_VERSION=121
cmake --build build --target subsdk9_meta
```

The output is `build/subsdk9` plus `build/main.npdm`. The release profile compiles
with `SOLO_HARNESS_TEXT=0` and `OVERLAY_DEBUG_HUD=0`.
The post-link import check rejects unresolved program symbols and the unbound
color constants that previously caused scan-time crashes.

Build once and use those same two output files for both installation layouts in
README.md. Do not compile separate emulator and Switch variants.

The `romfs/` files are intentionally absent from the source repository. See
[`romfs/README.md`](romfs/README.md) for the expected local directories.

## Host tests

The pure geometry and scheduling tests do not need exlaunch or any game files. They
fetch doctest v2.4.11 during configuration:

```powershell
pwsh -File tests/run_host_tests.ps1
```
