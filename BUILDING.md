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
- uv with Python 3.11 or newer
- UAM's NVN-capable shader compiler and NVIDIA nvdisasm for Maxwell SM53

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
  -DTOTK_VERSION=121 `
  -DFIDELITY_UAM=/path/to/uam.exe `
  -DFIDELITY_NVDISASM=/path/to/nvdisasm.exe `
  -DFIDELITY_UAM_RUNTIME=/path/to/compiler/runtime
cmake --build build --target subsdk9_meta
```

The output is `build/subsdk9` plus `build/main.npdm`. The release profile compiles
with `SOLO_HARNESS_TEXT=0` and `OVERLAY_DEBUG_HUD=0`.
`SURVEY_DEPTH_SCAN=ON`, `SURVEY_COMPACT_MAP=ON` and
`SURVEY_FIDELITY_PLAYGROUND=OFF` and `SURVEY_TUNING=OFF` select a shipping profile.
Leave `SURVEY_CONSTRAINED=OFF` for Regular (~440 m / 3 s), or set it `ON` for
Constrained (180 m / 7 s). All profiles use the same v0.1.4 version label.
The historical raycast and diagnostic profiles are retained for development,
not shipped as alternative binaries.
The post-link import check rejects unresolved program symbols and the unbound
color constants that previously caused scan-time crashes.

Build once per flavor and use its same two output files for both installation
layouts in README.md. Do not compile separate emulator and Switch variants.
Use separate build directories or reconfigure `SURVEY_CONSTRAINED` before rebuilding.

`tools/compile_shaders.py` verifies the external tool binaries before use:

- UAM SHA-256: `2bf51f6713b0219cdfbee6b8ab3c7b87169b85a001e1466129e44359f13aa6fb`
- nvdisasm 12.6.77 SHA-256: `1138409fc6d4202c533e357a55668e11186a114b84d345a8438a84e6216d58c0`

The pinned UAM executable comes from KillzXGaming/ShaderLibrary at commit
`790dd2e501926e536a9193907d1256d446104162`; its NVN compiler fork is
https://github.com/KillzXGaming/uam. Stock deko3d UAM is not interchangeable.
nvdisasm comes from NVIDIA's CUDA 12.6.77 Windows nvdisasm archive.

The compiler emits `build/generated/shader-receipt.json` with shader and tool
hashes. Resource bindings, unsupported instructions, local-memory traffic and
the module memory budget are checked during the build. Shader sources are in
`shaders/`; no precompiled custom shader is required in the source checkout.
Source paths and comment line numbers affect the GNU build identifier; compare
loaded sections separately from that identifier when checking reproducibility.

The `romfs/` files are intentionally absent from the source repository. See
[`romfs/README.md`](romfs/README.md) for the expected local directories.

## Host tests

The pure geometry and scheduling tests do not need exlaunch or any game files. They
fetch doctest v2.4.11 during configuration:

```powershell
pwsh -File tests/run_host_tests.ps1
```
