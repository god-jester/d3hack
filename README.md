# D3Hack

**The first-ever runtime mod for Diablo III on Nintendo Switch.**

Seasons offline. Challenge Rifts without servers. Community events on demand. Loot table tweaks, instant crafting, and dozens of QoL options. All running inside the game process: native C++ engine hooks for real-time game modification.

Want a quick feel for everything you can toggle? Check the example config: [`examples/config/d3hack-nx/config.toml`](examples/config/d3hack-nx/config.toml).

> Note: This project is for research, modding, and offline experimentation. Respect the game, other players, and applicable laws/ToS.

## Releases

Grab the latest build from the **Releases** section. No build environment required. Just drop the files into your emulator or Atmosphère setup and play.

---

## What It Does

d3hack-nx is an exlaunch-based module that hooks into D3 at runtime. While similar in spirit to cheat codes, it operates natively and independently of Atmosphère’s dmnt cheat sysmodule (EdiZon-style cheats) or emulator cheat managers, modifying game behavior live to enable features previously impossible offline.

| Category | Highlights & Technical Specifics |
|----------|----------------------------------|
| **Offline Seasons** | Force any season number/state locally via `s_varSeasonNum`. Full seasonal features and themes without NSO. |
| **Challenge Rifts** | Inject protobuf blobs from SD by hooking `sOnGetChallengeRiftData`. Pick a specific week or randomize from a pool. |
| **Community Events** | Toggle 20+ `c_szConfigSwap` flags: Double Goblins, Royal Grandeur, Ethereals, Soul Shards, and Fourth Cube Slot. |
| **Loot Research** | Control `DisableAncientDrops`, `ForcedILevel`, and `TieredLootRunLevel` (GR logic) for drop table experimentation. |
| **QoL Cheats** | Instant portal/crafts, `MovementSpeedMultiplier`, `NoCooldowns`, and `EquipAnySlot` logic for streamlined gameplay. |
| **Visuals & Performance** | Engine overlays for FPS/DDM labels and `PatchResolutionTargets` to force 1080p or specific dynamic res bounds. |
| **Safety & Core** | Lightweight `exl::hook` implementation that verifies game version `2.7.6.90885` before applying patches. |
| **Debug Overlays** | On-screen FPS, variable-res readout, build watermarks, DDM labels. |

## Features & Examples
*   Offline Seasons Mode
    *   Forces Seasons to be active even offline by stubbing network checks and setting live XVar flags (`s_varSeasonsOverrideEnabled`, `s_varSeasonNum`, `s_varSeasonState`).
    *   Injects season timing/config without servers by calling the game's file callback with `c_szSeasonSwap`/`c_szConfigSwap` data (see `source/program/d3/setting.hpp`).

*   Offline Challenge Rifts (Weekly) Emulation
    *   Hooks `sOnGetChallengeRiftData` and feeds protobuf messages for weekly content.
    *   Controlled by `[challenge_rifts]` in `config.toml` (enable/disable, random vs fixed, range start/end).

*   Community Buffs & Events (toggle-able)
    *   Enables many special-event flags even out of season; most toggles live in `c_szConfigSwap` and `PatchForcedEvents()` (see `source/program/d3/patches.hpp`).

*   Loot/Gameplay Research Tools
    *   Loot controls and QoL helpers live in `PatchConfig` (see `source/program/config.hpp`).

*   Reverse-Engineered Type & Symbol DB
    *   D3 types live under `source/program/d3/types/`; symbols under `source/program/symbols/*.hpp`; hook helpers under `source/lib/hook/*`.

---

## Quick Start

### 1. Download & Install
*   Grab the release archive.
*   Copy `subsdk9` and `main.npdm` to:
    *   **Atmosphère**: `atmosphere/contents/01001b300b9be000/exefs/`
    *   **Ryujinx/Yuzu**: The appropriate mods folder for title ID `01001b300b9be000`.

### 2. Configure
Place `config.toml` at:
*   `sdmc:/config/d3hack-nx/config.toml` (hardware)
*   Or the equivalent path in your emulator's virtual SD.

Config is checked in order:
*   `scratch:/config/d3hack-nx/config.toml`
*   `sdmc:/config/d3hack-nx/config.toml`

See `examples/config/d3hack-nx/config.toml` for all options.
*   `[seasons]`: Control `SeasonNumber` and active state.
*   `[challenge_rifts]`: Toggle `MakeRiftsRandom` or define specific `RiftRange` limits.
*   `[events]`: Enable individual buffs like `RoyalGrandeur`, `Pandemonium`, and `DarkAlchemy`.
*   `[rare_cheats]`: QoL tweaks including `DropAnyItems`, `SocketGemsToAnySlot`, and `NoCooldowns`.
*   `[overlays]`: Toggle specific on-screen elements like `FPSLabel` and `BuildLockerWatermark`.
*   `[loot_modifiers]`: Drop table research tools for `SuppressGiftGeneration` or `ForcedILevel`.
*   `[debug]`: Advanced controls for error traces and debug flags.

### 3. Challenge Rift Data
To enable offline Challenge Rifts, place captured weekly files under `sdmc:/config/d3hack-nx/rift_data/`:
```
challengerift_config.dat
challengerift_0.dat
...
challengerift_19.dat
```
The hooks in `source/program/d3/hooks/debug.hpp` will intercept the game's network requests and feed it these local binaries instead.

Tip: capture real weekly files once, then iterate offline instantly without reconnecting.

Example tree to copy to SD/emulator: `examples/config/d3hack-nx/rift_data/` (contains old captures and a README).
Import helper: `python3 tools/import_challenge_dumps.py --src ~/dumps --dst examples/config/d3hack-nx/rift_data --dry-run` then rerun without `--dry-run`.

### 4. Launch
Start D3 normally. The mod loads automatically, verifies game version `2.7.6.90885`, and applies your config.

---

## Building from Source

**Requirements**: devkitPro with devkitA64 + libnx (`$DEVKITPRO` must be set); Python 3 (optional `ftputil` for FTP deployments).

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
*   `source/` — gameplay/engine hooks and helpers (subfolders are modules).
*   `include/` — public headers (vendored libs live under `include/tomlplusplus/`).
*   `misc/mk` and `misc/scripts/` — build and deploy logic (Makefile includes these).
*   `deploy/` — post-build outputs (`subsdk9`, `main.npdm`).
*   `config.mk` — your local build/deploy settings (PROGRAM_ID, deploy targets, flags).

## Build Details
*   Toolchain flags: `-Wall -Werror -Ofast`, `gnu++23`, no exceptions/RTTI.
*   Artifacts are packaged via `misc/scripts/post-build.sh` and staged to `deploy/`.
*   Additional targets:
    *   `make clean` — remove artifacts
    *   `make npdm-json` — regen NPDM JSON (from `misc/npdm-json/*`)

## Development Notes
*   Add new functionality under `source/<module>/`; keep shared headers in `include/`.
*   Avoid modifying vendor headers under `include/tomlplusplus/`.

## Safety & Fair-Play
*   Use on your own device and content. Don’t redistribute game assets or proprietary files.
*   Don’t take mods into online play; it’s disrespectful and risks bans.
*   Scripts used by `make deploy-sd` use `sudo mount`; read them before running.

## Compatibility
*   Target game build: `2.7.6.90885` (see `source/program/d3/setting.hpp`). Offsets and patches are tied to this version; other versions will need symbol/offset updates.
    *   A lightweight signature guard checks a few known instruction/string bytes at startup and aborts if they do not match this build.

---

## Technical Highlights

*   **Type Database**: Over 1.2MB of reverse-engineered structs and enums in `source/program/d3/types/`.
*   **Hooking**: Uses `exl::hook::Trampoline` and `exl::hook::MakeInline` for clean detours.
*   **Offsets**: All offsets are static and verified against build `2.7.6.90885`. A mismatch triggers an immediate abort to prevent corruption.
*   **Config**: Runtime TOML parsing via [tomlplusplus](https://github.com/marzer/tomlplusplus), mapped directly to the global `PatchConfig` struct.

---

## Credits

Created by: **jester** (also behind [D3StudioFork](https://github.com/god-jester/D3StudioFork) and well-known D3 cheat releases) and built on [exlaunch](https://github.com/shadowninja108/exlaunch).
