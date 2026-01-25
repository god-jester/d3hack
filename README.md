# D3Hack

<img width="2560" height="1440" alt="image" src="https://github.com/user-attachments/assets/5154f973-8ea4-46e8-b419-f8b06422ae28" />

---

**Runtime modding for Diablo III on Nintendo Switch.**

Seasons offline. Challenge Rifts without servers. Community events on demand. Loot table tweaks, instant crafting, and quality-of-life tools. All running inside the game process via native C++ hooks.

**Current version: D3Hack v3.0**

Want the full toggle list? Start with the example config: `examples/config/d3hack-nx/config.toml`.
See: [Resolution Hack (ResHack) overview](#resolution-hack-reshack).

## Contents

- [Highlights](#highlights-current)
- [Resolution Hack (ResHack)](#resolution-hack-reshack)
- [What It Does](#what-it-does)
- [Quick Start](#quick-start)
- [Season Theme Mapping](#season-theme-mapping-14-22-24-37)
- [Building from Source](#building-from-source)
- [Validation (Dev)](#validation-dev)
- [Known Limits](#known-limits)
- [Project Structure](#project-structure)
- [Compatibility & Safety](#compatibility--safety)
- [Feature Map (Code Pointers)](#feature-map-code-pointers)

---

## Highlights (Current)

- **Seasons + events via XVars**: season number/state and event flags are applied with native XVar setters after config load.
- **Optional publisher file overrides**: Config/Seasons/Blacklist retrieval hooks can replace online files for offline play or testing.
- **Season event map**: one switch to auto-enable the correct seasonal theme flags for Seasons 14-22 and 24-37.
- **Challenge Rifts offline fix**: SD card protobufs now parse at full size (previous blz::string ctor issue resolved).
- **Safer offline UX**: hides "Connect to Diablo Servers" and network hints when AllowOnlinePlay = false.
- **Resolution Hack (ResHack)**: 1080p/1440p/2160p output is stable with internal RT clamping and extra heap headroom.
- **GUI overlay upgrades**: dockable windows + menu tools, touch swipe open/close, layout persistence, overlay labels rendered by ImGui, optional left-stick passthrough.
- **CMake presets**: CMakePresets.json mirrors the Makefile pipeline for devkitA64.

---

## Resolution Hack (ResHack)

ResHack keeps the swapchain output at your chosen size while clamping internal render targets (RTs) for stability. It is designed for true high-res output, not UI-only scaling.

Terminology
- OutputTarget and ClampTextureResolution are vertical heights in pixels; width is derived as 16:9.
- OutputHandheldScale is a percent (0 = auto/stock), and OutputTargetHandheld is derived when set to 0.

Current baseline (validated 2026-01-20)
- OutputTarget = 2160 (3840x2160).
- ClampTextureResolution = 1152 (2048x1152 internal).
- Graphics persistent heap bumped early (+256 MB, always on).
- NX64NVNHeap EarlyInit headroom and largest pool max_alloc bumped when output height > 1280.

### How it works

- **Config**: `[resolution_hack]` in `config.toml` sets OutputTarget, OutputHandheldScale (percent; 0=auto/stock), OutputTargetHandheld (derived), SpoofDocked, MinResScale, MaxResScale, ClampTextureResolution (0=off, 100-9999), and ExperimentalScheduler.
- **Config (extra)**: `[resolution_hack.extra]` can override display mode fields (MSAALevel, BitDepth, RefreshRate, and window/UI/render dimensions).
- **Patches**: PatchResolutionTargets rewrites the display mode table to match OutputTarget and applies NVN heap headroom when output height > 1280.
- **Hooks**: NVNTexInfoCreateHook clamps oversized internal textures while keeping swapchain textures output-sized.
- **Early heap**: PatchGraphicsPersistentHeapEarly bumps the graphics persistent heap before NVN init.

### Recommended settings (ResHack)
```toml
[resolution_hack]
SectionEnabled = true
OutputTarget = 2160
OutputHandheldScale = 80
SpoofDocked = false
ClampTextureResolution = 1152
MaxResScale = 100 # Adjust max scale for better performance while keeping native UI
MinResScale = 85 # Game default min scale is 70
```

### Optional display mode overrides
```toml
[resolution_hack.extra]
MSAALevel = -1
BitDepth = -1
RefreshRate = -1
```

### Notes
- 1080 output stays at or below 2048x1152, so the clamp path is usually inactive.
- If you raise the clamp too high at 1440/2160 output, the old crash signature can return (DisplayInternalError + InvalidMemoryRegionException).
- Overlay labels (FPS/variable-res/DDM) are rendered by the GUI overlay; they require `[gui].Enabled = true` and overlays toggles.

---

## What It Does

d3hack-nx is an exlaunch-based module that hooks D3 at runtime. It modifies game behavior live without relying on Atmosphere dmnt cheats or emulator cheat managers.

| Category | Highlights & Technical Notes |
|----------|------------------------------|
| **Offline Seasons** | Set season number/state via XVar updates + seasonal UI patches; optional seasons file override for offline parity. |
| **Season Theme Map** | SeasonMapMode can apply the official theme flags for Seasons 14-22 and 24-37, optionally OR'd with your custom flags. |
| **Challenge Rifts** | Hooks the challenge rift callback and injects protobuf blobs from SD (`rift_data`). |
| **Community Events** | Toggle 20+ flags (Ethereals, Soul Shards, Fourth Cube Slot, etc.) via runtime XVar updates. |
| **Loot Research** | Control Ancient/Primal drops, GR tier logic, and item level forcing for drop-table experiments. |
| **QoL Cheats** | Instant portal/crafts, movement speed multiplier, no cooldowns, equip-any-slot, and more. |
| **Visuals & Performance** | FPS/DDM overlays and resolution targets for 1080p or specific dynamic-res bounds. |
| **GUI Overlay** | Dockable ImGui windows with menu tools (show/hide/reset layout, notifications), touch swipe open/close, layout saved to `sd:/config/d3hack-nx/gui_layout.ini`, optional left-stick passthrough while the overlay is open, and overlay labels drawn by the GUI. |
| **Safety & Core** | Patches are gated by a lightweight signature guard for game build 2.7.6.90885. |

---

## Quick Start

### 1) Choose the right release asset

There are two release zips:
- **Atmosphere LayeredFS (Switch hardware)**: contains `sdcard/` with Atmosphere + config paths.
- **Emulators / mod managers**: contains `01001B300B9BE000/` (mod folder) and `sdcard/` (config).

Title ID: `01001B300B9BE000`.

### 2) Install

**Atmosphere LayeredFS (Switch hardware)**
- Copy the `sdcard/` folder contents to the root of your SD card.
- Exefs path: `sd:/atmosphere/contents/01001B300B9BE000/exefs/`
- Romfs path: `sd:/atmosphere/contents/01001B300B9BE000/romfs/`
- If you use Alchemist, install the exefs and romfs folders under:
  `/switch/.packages/Alchemist/contents/Diablo III - D3Hack/01001B300B9BE000/`

**Emulators / mod managers**
- Copy the `01001B300B9BE000/` folder into your mod manager or emulator mod root.
- Example paths:
  - Ryujinx (Windows): `%AppData%\Ryujinx\mods\contents\01001B300B9BE000\d3hack\`
  - Ryujinx (macOS): `~/Library/Application Support/Ryujinx/mods/contents/01001B300B9BE000/d3hack/`
  - Yuzu (Windows): `%AppData%\yuzu\load\01001B300B9BE000\d3hack\`
  - Yuzu (macOS): `~/Library/Application Support/yuzu/load/01001B300B9BE000/d3hack/`

### 3) Configure

The release zip includes `sdcard/config/d3hack-nx/config.toml`
and `sdcard/config/d3hack-nx/rift_data/`. Copy the `sdcard/config/`
folder to your target SD root.

- `sd:/config/d3hack-nx/config.toml` (hardware)
- Ryujinx (Windows): `%AppData%\Ryujinx\sdcard\config\d3hack-nx\config.toml`
- Ryujinx (macOS): `~/Library/Application Support/Ryujinx/sdcard/config/d3hack-nx/config.toml`
- Yuzu (Windows): `%AppData%\yuzu\sdmc\config\d3hack-nx\config.toml`
- Yuzu (macOS): `~/Library/Application Support/yuzu/sdmc/config/d3hack-nx/config.toml`

Edit `config.toml` after copying.

Key sections:

- `[resolution_hack]`: OutputTarget, OutputHandheldScale (percent),SpoofDocked, ClampTextureResolution, MinResScale, MaxResScale, ExperimentalScheduler.
- `[resolution_hack.extra]`: display mode overrides (MSAALevel, BitDepth, RefreshRate, window/UI/render dimensions).
- `[seasons]`: SeasonNumber, AllowOnlinePlay, SpoofPtr.
- `[events]`: seasonal flags + SeasonMapMode (MapOnly, OverlayConfig, Disabled).
- `[challenge_rifts]`: enable/disable, randomize, or define a range.
- `[rare_cheats]`, `[overlays]`, `[debug]`.
- `[gui]`: Enabled, Visible, AllowLeftStickPassthrough, Language (override).

### 4) (Optional) Challenge Rift data

Put weekly protobuf dumps under `sd:/config/d3hack-nx/rift_data/` (00-99):

```
challengerift_config.dat
challengerift_00.dat
...
challengerift_99.dat
```

The hook in `source/program/d3/hooks/debug.hpp` intercepts the network callback and feeds local protobufs.

Tip: capture real weekly files once, then iterate offline instantly. There is a helper: `python3 tools/import_challenge_dumps.py --src ~/dumps --dst examples/config/d3hack-nx/rift_data --dry-run` then rerun without `--dry-run`.

### 5) Launch

Start D3 normally. The mod verifies build 2.7.6.90885, loads config, then applies patches/hooks.

---

## Version Compatibility (2.7.6 Pin)

D3Hack targets game build 2.7.6.90885. If you are on any newer game update, you can make it compatible by downgrading to 2.7.6 via LayeredFS. This is done by adding the 2.7.6 files below into your layeredfs exefs/romfs paths.

Files to layer (2.7.6):

```
exefs/main
exefs/main.npdm
romfs/CPKs/CoreCommon.cpk
romfs/CPKs/ServerCommon.cpk
```

Known 2.7.6 file sizes + MD5 (Dec 26 2023):

```
9.7M  Dec 26 2023  c3d386af84779a9b6b74b3a3988193d2  exefs/main
1.5K  Dec 26 2023  b02fdd816addff5bb72fcdc2c4f719ef  exefs/main.npdm
76M   Dec 26 2023  618b6dffc4cf7c4da98ca47529a906c8  romfs/CPKs/CoreCommon.cpk
5.4M  Dec 26 2023  de80fce3642d9cde15147af544877983  romfs/CPKs/ServerCommon.cpk
```

These files are shared on Discord and are easy to acquire.

## Season Theme Mapping (14-22, 24-37)

Set `[events].SeasonMapMode` to map the correct theme flags for a given season number:

- `MapOnly`: apply the season's mapped theme, ignore event flags.
- `OverlayConfig`: apply the map, then OR with flags in your config.
- `Disabled`: skip mapping and use only your configured flags.

---

## Building from Source

**Requirements**: devkitPro with devkitA64 + libnx (`$DEVKITPRO` set); CMake >= 3.21; Python 3 (optional `ftputil` for FTP deploys).

Fetch submodules once:

```bash
git submodule update --init --recursive
```

Configure build/deploy in `config.mk` by setting the deploy targets you plan to use (FTP IP/port, `RYU_*`, `YUZU_PATH`).
For CMake-only overrides, copy `config.cmake.template` to `config.cmake`.

```bash
# Build
make                    # or ./exlaunch.sh build
cmake --preset switch
cmake --build --preset switch

# Deploy
make deploy-ryu         # Ryujinx
make deploy-yuzu        # Yuzu
make deploy-ftp         # Switch via FTP
make deploy-sd          # SD card (uses sudo mount)

# Clean
make clean
```

Outputs land in `deploy/`:
- `exefs/subsdk9` and `exefs/main.npdm`
- `romfs/d3gui/` assets (`imgui.bin`, fonts, translations)

### Include What You Use (IWYU)

The CMake build can run IWYU via the standard variables:
`CMAKE_C_INCLUDE_WHAT_YOU_USE` and `CMAKE_CXX_INCLUDE_WHAT_YOU_USE`.
If you set `EXL_ENABLE_IWYU=ON`, the project will auto-populate those
variables (unless you already set them) and propagate them to the target
properties `C_INCLUDE_WHAT_YOU_USE` and `CXX_INCLUDE_WHAT_YOU_USE`.

Example (config.cmake):
```cmake
set(EXL_ENABLE_IWYU ON)
set(EXL_IWYU_BINARY "/usr/local/bin/include-what-you-use")
set(EXL_IWYU_MAPPING_FILE "/path/to/iwyu/mapping.imp")
set(EXL_IWYU_ARGS
    "-Xiwyu" "--verbose=3"
)
```

To generate the symbol database used by clang-include-fixer:
```bash
cmake --build --preset switch --target iwyu-symbols
```

The symbol scan runs clang tooling against the Switch target triple
(`aarch64-none-elf`) and adds `-Wno-error` by default so warnings do not
abort the scan. Override with `EXL_IWYU_TARGET` or `EXL_IWYU_SYMBOLS_EXTRA_ARGS`
in `config.cmake` if needed.

There is also a CMake preset that turns IWYU on:
```bash
cmake --preset switch-iwyu
cmake --build --preset switch-iwyu
```

---

## Validation (Dev)

- `make -j`
- If runtime behavior changes: `make ryu-tail 2> /dev/null` and confirm `[D3Hack|exlaunch] ShellInitialized` appears within ~10s.
- If GUI behavior changes: wait ~10s after ShellInitialized, then `make ryu-screenshot`.

---

## Known Limits

- 2160 output is stable with internal RT clamping enabled.
- Raising the clamp too high at 1440/2160 output can trigger DisplayInternalError + InvalidMemoryRegionException.
- Overlay labels (FPS/variable-res/DDM) are rendered by the GUI overlay; they require `[gui].Enabled = true` and overlays toggles.

---

## Project Structure

- `source/` - gameplay/engine hooks and helpers (modules live here)
- `include/` - public headers (`include/tomlplusplus/` submodule)
- `source/third_party/imgui/` - Dear ImGui submodule
- `cmake/` - CMake toolchain and build helpers
- `data/` - romfs assets (`imgui.bin`, fonts, translations)
- `misc/scripts/` - build/deploy logic (Makefile includes these)
- `deploy/` - post-build outputs (`exefs/`, `romfs/`)
- `config.mk` - local build/deploy settings

---

## Compatibility & Safety

- **Target build**: 2.7.6.90885. Offsets and patches are tied to this version.
- A lightweight signature guard checks known bytes at startup and aborts on mismatch.
- Use offline; do not take mods into online play.

---

## Feature Map (Code Pointers)

- **Config load + defaults**: `source/program/config.cpp` (LoadPatchConfig, ApplySeasonEventMapIfNeeded).
- **Config schema + defaults**: `source/program/config.hpp` (PatchConfig, defaults).
- **Season/event XVar toggles**: `source/program/d3/patches.hpp` (PatchDynamicSeasonal, PatchDynamicEvents).
- **Season/event publisher overrides**: `source/program/d3/hooks/season_events.hpp` (ConfigFileRetrieved, SeasonsFileRetrieved, BlacklistFileRetrieved).
- **Season theme map**: `source/program/config.cpp` (BuildSeasonEventMap).
- **Challenge rifts**: `source/program/d3/_util.hpp` (PopulateChallengeRiftData) + `source/program/d3/hooks/debug.hpp` (ChallengeRiftCallback).
- **Resolution Hack (ResHack)**: `source/program/d3/hooks/resolution.hpp` + `source/program/d3/patches.hpp`.
- **Overlay labels (FPS/VarRes/DDM)**: `source/program/gui2/ui/overlay.cpp` (RenderOverlayLabels) and overlay toggles in `source/program/config.hpp`.
- **Hook gating + boot flow**: `source/program/main.cpp` (MainInit, SetupSeasonEventHooks).
- **GUI overlay (dockspace/menu/layout)**: `source/program/gui2/ui/overlay.*`, `source/program/gui2/ui/window.*`, `source/program/gui2/ui/windows/*` (layout persisted at `sd:/config/d3hack-nx/gui_layout.ini`).
- **ImGui romfs assets**: `data/` packaged to `deploy/romfs/d3gui/` by `misc/scripts/post-build.sh`.

---

## Technical Highlights

- **Type Database**: Over 1.2MB of reverse-engineered structs and enums in `source/program/d3/types/` (now split into focused modules with `namespaces.hpp` as the aggregator).
- **Hooking**: Uses `exl::hook::Trampoline` and `exl::hook::MakeInline` for clean detours.
- **Offsets**: Centralized in a versioned lookup table (DEFAULT pinned to 2.7.6.90885); signature guard + version checks abort on mismatch.
- **Config**: Runtime TOML parsing via tomlplusplus, mapped directly to the global PatchConfig struct.

---

## Credits

- D3Hack created by **jester** (also behind [D3StudioFork](https://github.com/god-jester/D3StudioFork) and well-known D3 cheat releases)
- [exlaunch](https://github.com/shadowninja108/exlaunch) created by **Shadow**.
- [ImGui](https://github.com/ocornut/imgui) by ocornut.

### Inspiration & References

- [TeamLumi/Luminescent_ExLaunch](https://github.com/TeamLumi/Luminescent_ExLaunch)
- [RedBoxing/Starlight](https://github.com/RedBoxing/Starlight)
- [Amethyst-szs/smo-lunakit](https://github.com/Amethyst-szs/smo-lunakit)
- [Martmists-GH/BDSP_Rombase](https://github.com/Martmists-GH/BDSP_Rombase)
