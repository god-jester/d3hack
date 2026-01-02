# D3Hack

**Runtime modding for Diablo III on Nintendo Switch.**

Seasons offline. Challenge Rifts without servers. Community events on demand. Loot table tweaks, instant crafting, and quality-of-life tools. All running inside the game process via native C++ hooks.

Want the full toggle list? Start with the example config: [`examples/config/d3hack-nx/config.toml`](examples/config/d3hack-nx/config.toml).

> This project is for research, modding, and offline experimentation. Respect the game, other players, and applicable laws/ToS.

---

## Highlights (Jan 2026)

- **Publisher file overrides**: intercepts Config/Seasons/Blacklist retrieval and injects local data in-place (reliable offline behavior).
- **Season event map**: one switch to auto-enable the correct seasonal theme flags for **Seasons 14–37**.
- **Challenge Rifts offline fix**: SD card protobufs now parse at full size (previous `blz::string` ctor issue resolved).
- **Dynamic seasonal patching**: season number/state and UI gating are set at runtime after config load.
- **Safer offline UX**: hides “Connect to Diablo Servers” and network hints when `AllowOnlinePlay = false`.

---

## What It Does

d3hack-nx is an exlaunch-based module that hooks D3 at runtime. It modifies game behavior live without relying on Atmosphère’s dmnt cheats or emulator cheat managers.

| Category | Highlights & Technical Notes |
|----------|------------------------------|
| **Offline Seasons** | Force seasons on locally, set season number/state via XVars, and hide network prompts. |
| **Season Theme Map** | `SeasonMapMode` can apply the official theme flags for Seasons 14–37, optionally OR’d with your custom flags. |
| **Challenge Rifts** | Hooks the challenge rift callback and injects protobuf blobs from SD (`rift_data`). |
| **Community Events** | Toggle 20+ flags (Ethereals, Soul Shards, Fourth Cube Slot, etc.) with runtime XVar updates. |
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
    `%AppData%\yuzu\load\01001B300B9BE000\d3hack\exefs\` (Windows)
    `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/exefs/` (macOS)

### 2) Configure
Place `config.toml` at:
- `sd:/config/d3hack-nx/config.toml` (hardware)
- **Ryujinx/Yuzu**:
    `%AppData%\yuzu\sdmc\config\d3hack-nx\config.toml` (Windows)
    `~/Library/Application Support/Ryujinx/sdcard/config/d3hack-nx/config.toml` (macOS)

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

- **Season overrides**: `source/program/d3/hooks/debug.hpp` (Config/Seasons/Blacklist retrieval hooks)
- **Dynamic seasonal patching**: `source/program/d3/patches.hpp` (`PatchDynamicSeasonal`)
- **Season theme map**: `source/program/config.cpp` (`BuildSeasonEventMap`)
- **Challenge rifts**: `source/program/d3/_util.hpp` + `source/program/d3/hooks/debug.hpp`
- **Config load**: `source/program/config.cpp` (`LoadPatchConfig`)

---

## Technical Highlights

- **Type Database**: Over 1.2MB of reverse-engineered structs and enums in `source/program/d3/types/`.
- **Hooking**: Uses `exl::hook::Trampoline` and `exl::hook::MakeInline` for clean detours.
- **Offsets**: All offsets are static and verified against build `2.7.6.90885`. A mismatch triggers an immediate abort to prevent corruption.
- **Config**: Runtime TOML parsing via [tomlplusplus](https://github.com/marzer/tomlplusplus), mapped directly to the global `PatchConfig` struct.

---

## Credits

Created by: **jester** (also behind [D3StudioFork](https://github.com/god-jester/D3StudioFork) and well-known D3 cheat releases) and built on [exlaunch](https://github.com/shadowninja108/exlaunch).
