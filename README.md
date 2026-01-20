# D3Hack

**Runtime modding for Diablo III on Nintendo Switch.**

Seasons offline. Challenge Rifts without servers. Community events on demand. Loot table tweaks, instant crafting, and quality-of-life tools. All running inside the game process via native C++ hooks.

**Current version: D3Hack v3.0**

Want the full toggle list? Start with the example config: `examples/config/d3hack-nx/config.toml`.
New: [Resolution Hack (ResHack) overview](#resolution-hack-reshack).

---

## Highlights (Current)

- **Seasons + events via XVars**: season number/state and event flags are applied with native XVar setters after config load.
- **Optional publisher file overrides**: Config/Seasons/Blacklist retrieval hooks can replace online files for offline play or testing.
- **Season event map**: one switch to auto-enable the correct seasonal theme flags for Seasons 14-22 and 24-37.
- **Challenge Rifts offline fix**: SD card protobufs now parse at full size (previous blz::string ctor issue resolved).
- **Safer offline UX**: hides "Connect to Diablo Servers" and network hints when AllowOnlinePlay = false.
- **Resolution Hack (ResHack)**: 1080p/1440p/2160p output is stable with internal RT clamping and extra heap headroom.
- **CMake build**: CMake presets mirror the Makefile pipeline for devkitA64.

---

## Resolution Hack (ResHack)

ResHack keeps the swapchain output at your chosen size while clamping internal render targets (RTs) for stability. It is designed for true high-res output, not UI-only scaling.

Current baseline (2026-01-20)
- OutputTarget = 2160 (3840x2160).
- ClampTextureResolution = 1152 (2048x1152 internal).
- Graphics persistent heap bumped early (+256 MB, always on).
- NX64NVNHeap EarlyInit headroom and largest pool max_alloc bumped when output height > 1280.

How it works
- **Config**: `[resolution_hack]` in `config.toml` sets OutputTarget, MinResScale, ClampTextureResolution (0=off, 100-9999), and ExperimentalScheduler.
- **Patches**: PatchResolutionTargets rewrites the display mode table to match OutputTarget and applies NVN heap headroom when output height > 1280.
- **Hooks**: NVNTexInfoCreateHook clamps oversized internal textures while keeping swapchain textures output-sized.
- **Early heap**: PatchGraphicsPersistentHeapEarly bumps the graphics persistent heap before NVN init.

Recommended settings (ResHack)
```
[resolution_hack]
SectionEnabled = true
OutputTarget = 2160
ClampTextureResolution = 1152
# MinResScale: keep your normal value (85 is a safe starting point).
```

Notes
- 1080 output stays at or below 2048x1152, so the clamp path is mostly inactive.
- If you raise the clamp too high at 1440/2160 output, the old crash signature can return (DisplayInternalError + InvalidMemoryRegionException).
- Label overlays (FPS/resolution/DDM) can flicker white in the main menu; disable overlays if it bothers you.

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
| **Safety & Core** | Patches are gated by a lightweight signature guard for game build 2.7.6.90885. |

---

## Quick Start

### 1) Install

- Grab the latest release archive.
- Copy `subsdk9` and `main.npdm` to the `exefs/` folder, and copy the `romfs/` folder (contains `d3gui/imgui.bin`, fonts, and translations).
- Atmosphere exefs: `atmosphere/contents/01001b300b9be000/exefs/`
- Atmosphere romfs: `atmosphere/contents/01001b300b9be000/romfs/`
- Ryujinx (Windows) exefs: `%AppData%\Ryujinx\mods\contents\01001B300B9BE000\d3hack\exefs\`
- Ryujinx (Windows) romfs: `%AppData%\Ryujinx\mods\contents\01001B300B9BE000\d3hack\romfs\`
- Ryujinx (macOS) exefs: `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/exefs`
- Ryujinx (macOS) romfs: `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/romfs`
- Yuzu (Windows) exefs: `%AppData%\yuzu\load\01001B300B9BE000\d3hack\exefs\`
- Yuzu (Windows) romfs: `%AppData%\yuzu\load\01001B300B9BE000\d3hack\romfs\`
- Yuzu (macOS) exefs: `~/Library/Application Support/yuzu/load/01001b300b9be000/d3hack/exefs`
- Yuzu (macOS) romfs: `~/Library/Application Support/yuzu/load/01001b300b9be000/d3hack/romfs`

### 2) Configure

Place `config.toml` at:

- `sd:/config/d3hack-nx/config.toml` (hardware)
- Ryujinx (Windows): `%AppData%\Ryujinx\sdcard\config\d3hack-nx\config.toml`
- Ryujinx (macOS): `~/Library/Application Support/Ryujinx/sdcard/config/d3hack-nx/config.toml`
- Yuzu (Windows): `%AppData%\yuzu\sdmc\config\d3hack-nx\config.toml`
- Yuzu (macOS): `~/Library/Application Support/yuzu/sdmc/config/d3hack-nx/config.toml`

Key sections:

- `[resolution_hack]`: OutputTarget, ClampTextureResolution, MinResScale, ExperimentalScheduler.
- `[seasons]`: SeasonNumber, AllowOnlinePlay, SpoofPtr.
- `[events]`: seasonal flags + SeasonMapMode (MapOnly, OverlayConfig, Disabled).
- `[challenge_rifts]`: enable/disable, randomize, or define a range.
- `[rare_cheats]`, `[overlays]`, `[debug]`.
- `[gui]`: Enabled, Visible, Language (override).

### 3) (Optional) Challenge Rift data

Put weekly files under `sd:/config/d3hack-nx/rift_data/`:

```
challengerift_config.dat
challengerift_00.dat
...
challengerift_99.dat
```

The hook in `source/program/d3/hooks/debug.hpp` intercepts the network callback and feeds local protobufs.

Tip: capture real weekly files once, then iterate offline instantly. There is a helper: `python3 tools/import_challenge_dumps.py --src ~/dumps --dst examples/config/d3hack-nx/rift_data --dry-run` then rerun without `--dry-run`.

### 4) Launch

Start D3 normally. The mod verifies build 2.7.6.90885, loads config, then applies patches/hooks.

---

## Season Theme Mapping (14-22, 24-37)

Set `[events].SeasonMapMode` to map the correct theme flags for a given season number:

- `MapOnly`: apply the season's mapped theme, ignore event flags.
- `OverlayConfig`: apply the map, then OR with flags in your config.
- `Disabled`: skip mapping and use only your configured flags.

---

## Building from Source

**Requirements**: devkitPro with devkitA64 + libnx (`$DEVKITPRO` set); Python 3 (optional `ftputil` for FTP deploys).

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

---

## Validation (Dev)

- `make -j`
- If runtime behavior changes: `make ryu-tail 2> /dev/null` and confirm `[D3Hack|exlaunch] ShellInitialized` appears within ~10s.
- If GUI behavior changes: wait ~10s after ShellInitialized, then `make ryu-screenshot`.

---

## Known Limits

- 2160 output is stable with internal RT clamping enabled.
- Raising the clamp too high at 1440/2160 output can trigger DisplayInternalError + InvalidMemoryRegionException.
- Label overlays (FPS/resolution/DDM) can flicker in the main menu; disable overlays if it bothers you.
- Use offline; do not take mods into online play.

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
- **Season/event XVar toggles**: `source/program/d3/patches.hpp` (PatchDynamicSeasonal, PatchDynamicEvents).
- **Season/event publisher overrides**: `source/program/d3/hooks/season_events.hpp` (ConfigFileRetrieved, SeasonsFileRetrieved, BlacklistFileRetrieved).
- **Season theme map**: `source/program/config.cpp` (BuildSeasonEventMap).
- **Challenge rifts**: `source/program/d3/_util.hpp` (PopulateChallengeRiftData) + `source/program/d3/hooks/debug.hpp` (ChallengeRiftCallback).
- **Resolution hack (ResHack)**: `source/program/d3/hooks/resolution.hpp` + `source/program/d3/patches.hpp`.
- **Overlays + watermark text**: `source/program/d3/patches.hpp` (PatchBuildlocker, PatchDDMLabels, PatchReleaseFPSLabel).
- **Hook gating + boot flow**: `source/program/main.cpp` (MainInit, SetupSeasonEventHooks).
- **ImGui romfs assets**: `data/` packaged to `deploy/romfs/d3gui/` by `misc/scripts/post-build.sh`.

---

## Technical Highlights

- **Type Database**: Over 1.2MB of reverse-engineered structs and enums in `source/program/d3/types/`.
- **Hooking**: Uses `exl::hook::Trampoline` and `exl::hook::MakeInline` for clean detours.
- **Offsets**: Centralized in a versioned lookup table (DEFAULT pinned to 2.7.6.90885); signature guard + version checks abort on mismatch.
- **Config**: Runtime TOML parsing via tomlplusplus, mapped directly to the global PatchConfig struct.

---

## Credits

- D3Hack created by jester (also behind D3StudioFork and well-known D3 cheat releases).
- exlaunch created by Shadow.
- ImGui by ocornut.

### Inspiration & References

- TeamLumi/Luminescent_ExLaunch
- RedBoxing/Starlight
- Amethyst-szs/smo-lunakit
- Martmists-GH/BDSP_Rombase
