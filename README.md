# D3Hack

**Runtime modding for Diablo III on Nintendo Switch.**

Seasons offline. Challenge Rifts without servers. Community events on demand. Loot table tweaks, instant crafting, and quality-of-life tools. All running inside the game process via native C++ hooks.

Want the full toggle list? Start with the example config: [`examples/config/d3hack-nx/config.toml`](examples/config/d3hack-nx/config.toml).
New: [1080p+ Output & Resolution Hack breakdown](#1080p-output--resolution-hack) (how the patches + hooks work, and why depth detachment is required at 1152p+).

---

## Highlights (Current)

- **Seasons + events via XVars**: season number/state and event flags are applied with native XVar setters after config load.
- **Optional publisher file overrides**: Config/Seasons/Blacklist retrieval hooks can replace online files for offline play or testing, but they are not required for the primary toggles.
- **Season event map**: one switch to auto-enable the correct seasonal theme flags for **Seasons 14–37**.
- **Challenge Rifts offline fix**: SD card protobufs now parse at full size (previous `blz::string` ctor issue resolved).
- **Safer offline UX**: hides “Connect to Diablo Servers” and network hints when `AllowOnlinePlay = false`.

---

## 1080p+ Output & Resolution Hack

This path makes **1920x1080** (and optional **2560x1440**) swapchain output practical while keeping internal RTs clamped to **<=2048** for stability. The critical fix is **depth detachment on the swapchain**; without it, a 2048x1152 depth attachment clamps the render area and the UI hard-clips. That hard-clip only showed up at **1152p+ output**; 1080p runs clean without depth detachment.

Breakdown:

- **Config**: `[resolution_hack]` in `config.toml` sets `OutputTarget` (`1080`/`1440`/`2160`), `MinResScale`, and `ClampTextureResolution` (0=off, 100-9999; default 1152, i.e. 2048x1152).
- **Patches**: `PatchResolutionTargets()` in `source/program/d3/patches.hpp` rewrites the default display mode pair, forces docked/perf checks, and nudges font point sizes for high-res output.
- **Hooks (shipping path)**: `source/program/d3/hooks/resolution.hpp` installs `PostFXInitClampDims` (internal RT clamp) plus `GfxViewportSetSwapchainDepthGate` + `GfxSetDepthTargetSwapchainFix` (detach the clamped swapchain depth target to prevent the hard-clip, without breaking inventory depth). These only install when output exceeds the internal clamp, so 1080p skips them.
- **Known issue (main menu text flicker)**: when label overlays are enabled and FPS or res scale fluctuates, the main menu text can flicker white as the text layer is re-created. This is how the engine works, and Blizzard never cared to fix it (the overlays were meant only for debug). **Fix: disable the label overlays** (FPS, resolution, etc.) in `config.toml` if it bothers you.
- **Known issue (1152p+)**: when output height exceeds **1152p** (e.g. **1440p**), the inventory/pause screen 3D model can lose its **front-most layer** (nearest geometry) with the depth-detach workaround. Fixes are being extensively tested; please avoid filing duplicate bug reports.
- **Notes**: UI basis/NDC/UIC conversion hooks and stencil overrides were tested but do **not** fix the clip, so they stay out of the shipping path.

---

## What It Does

d3hack-nx is an exlaunch-based module that hooks D3 at runtime. It modifies game behavior live without relying on Atmosphère’s dmnt cheats or emulator cheat managers.

| Category | Highlights & Technical Notes |
|----------|------------------------------|
| **Offline Seasons** | Set season number/state via XVar updates + seasonal UI patches; optional seasons file override for offline parity. |
| **Season Theme Map** | `SeasonMapMode` can apply the official theme flags for Seasons 14–37, optionally OR’d with your custom flags. |
| **Challenge Rifts** | Hooks the challenge rift callback and injects protobuf blobs from SD (`rift_data`). |
| **Community Events** | Toggle 20+ flags (Ethereals, Soul Shards, Fourth Cube Slot, etc.) via runtime XVar updates; optional config file override to mirror community buff files. |
| **Loot Research** | Control Ancient/Primal drops, GR tier logic, and item level forcing for drop-table experiments. |
| **QoL Cheats** | Instant portal/crafts, movement speed multiplier, no cooldowns, equip-any-slot, and more. |
| **Visuals & Performance** | FPS/DDM overlays and resolution targets for 1080p or specific dynamic-res bounds. |
| **Safety & Core** | Patches are gated by a lightweight signature guard for game build `2.7.6.90885`. |

---

## Quick Start

### 1) Install

- Grab the latest release archive.
- Copy `subsdk9` and `main.npdm` to:
  - **Atmosphère**: `atmosphere/contents/01001b300b9be000/exefs/`
  - **Ryujinx/Yuzu**:
    - Windows: `%AppData%\yuzu\load\01001B300B9BE000\d3hack\exefs\`
    - macOS: `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/exefs`

### 2) Configure

Place `config.toml` at:

- `sd:/config/d3hack-nx/config.toml` (hardware)
- **Ryujinx/Yuzu**:
  - Windows: `%AppData%\yuzu\sdmc\config\d3hack-nx\config.toml`
  - macOS: `~/Library/Application Support/Ryujinx/sdcard/config/d3hack-nx/config.toml`

Key sections:

- `[seasons]`: `SeasonNumber`, `AllowOnlinePlay`.
- `[events]`: seasonal flags + `SeasonMapMode` (`MapOnly`, `OverlayConfig`, `Disabled`).
- `[challenge_rifts]`: enable/disable, randomize, or define a range.
- `[rare_cheats]`, `[overlays]`, `[loot_modifiers]`, `[debug]`.

### 3) (Optional) Challenge Rift data

Put weekly files under `sd:/config/d3hack-nx/rift_data/`:

```
challengerift_config.dat
challengerift_00.dat
...
challengerift_99.dat
```

The hook in `source/program/d3/hooks/debug.hpp` intercepts the network callback and feeds local protobufs.

Tip: capture real weekly files once, then iterate offline instantly. There’s a helper available: `python3 tools/import_challenge_dumps.py --src ~/dumps --dst examples/config/d3hack-nx/rift_data --dry-run` then rerun without `--dry-run`.

### 4) Launch

Start D3 normally. The mod verifies build `2.7.6.90885`, loads config, then applies patches/hooks.

---

## Season Theme Mapping (14–37)

Set `[events].SeasonMapMode` to map the correct theme flags for a given season number:

- `MapOnly`: apply the season’s mapped theme, ignore event flags.
- `OverlayConfig`: apply the map, then OR with flags in your config.
- `Disabled`: skip mapping and use only your configured flags.

---

## Building from Source

**Requirements**: devkitPro with devkitA64 + libnx (`$DEVKITPRO` set); Python 3 (optional `ftputil` for FTP deploys).

Configure build/deploy in `config.mk` by setting the deploy targets you plan to use (FTP IP/port, `RYU_*`, `YUZU_PATH`).

```bash
# Build
make                    # or ./exlaunch.sh build

# Deploy
make deploy-ryu         # Ryujinx
make deploy-yuzu        # Yuzu
make deploy-ftp         # Switch via FTP
make deploy-sd          # SD card (uses sudo mount)

# Clean
make clean
```

Outputs land in `deploy/` as `subsdk9` and `main.npdm`.

---

## Project Structure

- `source/` — gameplay/engine hooks and helpers (modules live here)
- `include/` — public headers (vendored libs under `include/tomlplusplus/`)
- `misc/scripts/` — build/deploy logic (Makefile includes these)
- `deploy/` — post-build outputs (`subsdk9`, `main.npdm`)
- `config.mk` — local build/deploy settings

---

## Compatibility & Safety

- **Target build**: `2.7.6.90885`. Offsets and patches are tied to this version.
- A lightweight signature guard checks known bytes at startup and aborts on mismatch.
- Use offline; don’t take mods into online play.

---

## Feature Map (Code Pointers)

- **Config load + defaults**: [`source/program/config.cpp`](source/program/config.cpp) (`LoadPatchConfig`, `ApplySeasonEventMapIfNeeded`).
- **Season/event XVar toggles**: [`source/program/d3/patches.hpp`](source/program/d3/patches.hpp) (`PatchDynamicSeasonal`, `PatchDynamicEvents`).
- **Season/event publisher overrides**: [`source/program/d3/hooks/season_events.hpp`](source/program/d3/hooks/season_events.hpp) (`ConfigFileRetrieved`, `SeasonsFileRetrieved`, `BlacklistFileRetrieved`).
- **Season theme map**: [`source/program/config.cpp`](source/program/config.cpp) (`BuildSeasonEventMap`).
- **Challenge rifts**: [`source/program/d3/_util.hpp`](source/program/d3/_util.hpp) (`PopulateChallengeRiftData`) + [`source/program/d3/hooks/debug.hpp`](source/program/d3/hooks/debug.hpp) (`ChallengeRiftCallback`).
- **Resolution hack (1080p/1440p)**: [`source/program/d3/hooks/resolution.hpp`](source/program/d3/hooks/resolution.hpp) + [`source/program/d3/patches.hpp`](source/program/d3/patches.hpp) (`PatchResolutionTargets`).
- **Overlays + watermark text**: [`source/program/d3/patches.hpp`](source/program/d3/patches.hpp) (`PatchBuildlocker`, `PatchDDMLabels`, `PatchReleaseFPSLabel`).
- **Hook gating + boot flow**: [`source/program/main.cpp`](source/program/main.cpp) (`MainInit`, `SetupSeasonEventHooks`).

---

## Technical Highlights

- **Type Database**: Over 1.2MB of reverse-engineered structs and enums in `source/program/d3/types/`.
- **Hooking**: Uses `exl::hook::Trampoline` and `exl::hook::MakeInline` for clean detours.
- **Offsets**: Centralized in a versioned lookup table (DEFAULT pinned to `2.7.6.90885`); signature guard + version checks abort on mismatch to prevent corruption.
- **Config**: Runtime TOML parsing via [tomlplusplus](https://github.com/marzer/tomlplusplus), mapped directly to the global `PatchConfig` struct.

---

## Credits

- D3Hack created by **jester** (also behind [D3StudioFork](https://github.com/god-jester/D3StudioFork) and well-known D3 cheat releases) 
- [exlaunch](https://github.com/shadowninja108/exlaunch) created by **Shadow**.
- [ImGui](https://github.com/ocornut/imgui)

### Inspiration & References

- [TeamLumi/Luminescent_ExLaunch](https://github.com/TeamLumi/Luminescent_ExLaunch)
- [RedBoxing/Starlight](https://github.com/RedBoxing/Starlight)
- [Amethyst-szs/smo-lunakit](https://github.com/Amethyst-szs/smo-lunakit)
- [Martmists-GH/BDSP_Rombase](https://github.com/Martmists-GH/BDSP_Rombase)
