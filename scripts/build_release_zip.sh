#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $(basename "$0") <output-zip> <variant>" >&2
  echo "  variant: atmosphere | mod" >&2
  exit 2
}

if [ $# -lt 2 ]; then
  usage
fi

OUT_PATH="$1"
VARIANT="$2"
REPO_ROOT="${REPO_ROOT:-$(pwd)}"
if [[ "$OUT_PATH" != /* ]]; then
  OUT_PATH="$REPO_ROOT/$OUT_PATH"
fi
TITLE_ID="01001B300B9BE000"

case "$VARIANT" in
  atmosphere|mod)
    ;;
  *)
    echo "Unknown variant: $VARIANT" >&2
    usage
    ;;
esac

require_file() {
  local path="$1"
  if [ ! -f "$path" ]; then
    echo "Missing required file: $path" >&2
    exit 1
  fi
}

require_dir() {
  local path="$1"
  if [ ! -d "$path" ]; then
    echo "Missing required directory: $path" >&2
    exit 1
  fi
}

require_file "$REPO_ROOT/deploy/exefs/main.npdm"
require_file "$REPO_ROOT/deploy/exefs/subsdk9"
require_dir "$REPO_ROOT/deploy/romfs"
require_file "$REPO_ROOT/examples/config/d3hack-nx/config.toml"
require_dir "$REPO_ROOT/examples/config/d3hack-nx/rift_data"

TMP_DIR="$(mktemp -d /tmp/d3hack-release-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/sdcard/config/d3hack-nx/rift_data"
cp "$REPO_ROOT/examples/config/d3hack-nx/config.toml" \
  "$TMP_DIR/sdcard/config/d3hack-nx/"
cp "$REPO_ROOT/examples/config/d3hack-nx/rift_data/"* \
  "$TMP_DIR/sdcard/config/d3hack-nx/rift_data/"

if [ "$VARIANT" = "atmosphere" ]; then
  mkdir -p \
    "$TMP_DIR/sdcard/atmosphere/contents/$TITLE_ID/exefs" \
    "$TMP_DIR/sdcard/atmosphere/contents/$TITLE_ID/romfs"

  cp "$REPO_ROOT/deploy/exefs/main.npdm" "$REPO_ROOT/deploy/exefs/subsdk9" \
    "$TMP_DIR/sdcard/atmosphere/contents/$TITLE_ID/exefs/"
  cp -R "$REPO_ROOT/deploy/romfs/"* \
    "$TMP_DIR/sdcard/atmosphere/contents/$TITLE_ID/romfs/"

  cat > "$TMP_DIR/README.txt" <<'README'
### 1) Install (Atmosphere LayeredFS)
- Copy the `sdcard/` folder contents to the root of your SD card.
- Title ID: 01001B300B9BE000
- Exefs path:
  `sd:/atmosphere/contents/01001B300B9BE000/exefs/`
- Romfs path:
  `sd:/atmosphere/contents/01001B300B9BE000/romfs/`
- If you use Alchemist, install under:
  `/switch/.packages/Alchemist/contents/Diablo III - D3Hack/01001B300B9BE000/`

### 2) Configure
- Copy the `sdcard/config/` folder into the root of your SD card.
- This places the config at:
  `sd:/config/d3hack-nx/config.toml`
- Edit `config.toml` after copying.

### 3) Version Compatibility (2.7.6 Pin)
D3Hack targets game build 2.7.6.90885. If you are on a newer update, you can
layer the following 2.7.6 files via LayeredFS:
- exefs/main
- romfs/CPKs/CoreCommon.cpk
- romfs/CPKs/ServerCommon.cpk
Known 2.7.6 file sizes + MD5 (Dec 26 2023):
  9.7M  Dec 26 2023  c3d386af84779a9b6b74b3a3988193d2  exefs/main
  76M   Dec 26 2023  618b6dffc4cf7c4da98ca47529a906c8  romfs/CPKs/CoreCommon.cpk
  5.4M  Dec 26 2023  de80fce3642d9cde15147af544877983  romfs/CPKs/ServerCommon.cpk
README
else
  mkdir -p \
    "$TMP_DIR/$TITLE_ID/d3hack/exefs" \
    "$TMP_DIR/$TITLE_ID/d3hack/romfs"

  cp "$REPO_ROOT/deploy/exefs/main.npdm" "$REPO_ROOT/deploy/exefs/subsdk9" \
    "$TMP_DIR/$TITLE_ID/d3hack/exefs/"
  cp -R "$REPO_ROOT/deploy/romfs/"* \
    "$TMP_DIR/$TITLE_ID/d3hack/romfs/"

  cat > "$TMP_DIR/README.txt" <<'README'
### 1) Install (Emulators / Mod Managers)
- Copy the `01001B300B9BE000/` folder into your mod manager or emulator
  mod root.
- Example paths:
  - Ryujinx (Windows):
    `%AppData%\Ryujinx\mods\contents\01001B300B9BE000\d3hack\`
  - Ryujinx (macOS):
    `~/Library/Application Support/Ryujinx/mods/contents/01001B300B9BE000/d3hack/`
  - Yuzu (Windows):
    `%AppData%\yuzu\load\01001B300B9BE000\d3hack\`
  - Yuzu (macOS):
    `~/Library/Application Support/yuzu/load/01001B300B9BE000/d3hack/`

### 2) Configure
- Copy the `sdcard/config/` folder into the emulator SD root.
- That resolves to:
  - Ryujinx (Windows): `%AppData%\Ryujinx\sdcard\config\`
  - Ryujinx (macOS): `~/Library/Application Support/Ryujinx/sdcard/config/`
  - Yuzu (Windows): `%AppData%\yuzu\sdmc\config\`
  - Yuzu (macOS): `~/Library/Application Support/yuzu/sdmc/config/`
- Edit `config.toml` at `config/d3hack-nx/config.toml`.

### 3) Version Compatibility (2.7.6 Pin)
D3Hack targets game build 2.7.6.90885. If you are on a newer update, you can
layer the following 2.7.6 files via LayeredFS:
- exefs/main
- romfs/CPKs/CoreCommon.cpk
- romfs/CPKs/ServerCommon.cpk
Known 2.7.6 file sizes + MD5 (Dec 26 2023):
  9.7M  Dec 26 2023  c3d386af84779a9b6b74b3a3988193d2  exefs/main
  76M   Dec 26 2023  618b6dffc4cf7c4da98ca47529a906c8  romfs/CPKs/CoreCommon.cpk
  5.4M  Dec 26 2023  de80fce3642d9cde15147af544877983  romfs/CPKs/ServerCommon.cpk
README
fi

find "$TMP_DIR" -name '.DS_Store' -delete

mkdir -p "$(dirname "$OUT_PATH")"
rm -f "$OUT_PATH"
(cd "$TMP_DIR" && zip -r "$OUT_PATH" . -x "*.DS_Store")
