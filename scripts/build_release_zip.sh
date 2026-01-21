#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: $(basename "$0") <output-zip>" >&2
  exit 2
}

if [ $# -lt 1 ]; then
  usage
fi

OUT_PATH="$1"
REPO_ROOT="${REPO_ROOT:-$(pwd)}"
if [[ "$OUT_PATH" != /* ]]; then
  OUT_PATH="$REPO_ROOT/$OUT_PATH"
fi
TITLE_ID="01001b300b9be000"

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

mkdir -p \
  "$TMP_DIR/atmosphere/contents/$TITLE_ID/exefs" \
  "$TMP_DIR/atmosphere/contents/$TITLE_ID/romfs" \
  "$TMP_DIR/config/d3hack-nx/rift_data" \
  "$TMP_DIR/d3hack/exefs" \
  "$TMP_DIR/d3hack/romfs"

cp "$REPO_ROOT/deploy/exefs/main.npdm" "$REPO_ROOT/deploy/exefs/subsdk9" \
  "$TMP_DIR/atmosphere/contents/$TITLE_ID/exefs/"
cp "$REPO_ROOT/deploy/exefs/main.npdm" "$REPO_ROOT/deploy/exefs/subsdk9" \
  "$TMP_DIR/d3hack/exefs/"

cp -R "$REPO_ROOT/deploy/romfs/"* \
  "$TMP_DIR/atmosphere/contents/$TITLE_ID/romfs/"
cp -R "$REPO_ROOT/deploy/romfs/"* \
  "$TMP_DIR/d3hack/romfs/"

cp "$REPO_ROOT/examples/config/d3hack-nx/config.toml" \
  "$TMP_DIR/config/d3hack-nx/"
cp "$REPO_ROOT/examples/config/d3hack-nx/rift_data/"* \
  "$TMP_DIR/config/d3hack-nx/rift_data/"

cat > "$TMP_DIR/d3hack/README.txt" <<'README'
### 1) Install
- Grab the latest release archive.
- Copy `subsdk9` and `main.npdm` to:
  - **Ryujinx/Yuzu**:
    `%AppData%\yuzu\load\01001B300B9BE000\d3hack\exefs\` (Windows)
    `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/exefs/` (macOS)
- Copy the `romfs` folder to:
  - **Ryujinx/Yuzu**:
    `%AppData%\yuzu\load\01001B300B9BE000\d3hack\romfs\` (Windows)
    `~/Library/Application Support/Ryujinx/mods/contents/01001b300b9be000/d3hack/romfs/` (macOS)

### 2) Configure
Place `config.toml` at:
- **Ryujinx/Yuzu**:
    `%AppData%\yuzu\sdmc\config\d3hack-nx\config.toml` (Windows)
    `~/Library/Application Support/Ryujinx/sdcard/config/d3hack-nx/config.toml` (macOS)
README

find "$TMP_DIR" -name '.DS_Store' -delete

mkdir -p "$(dirname "$OUT_PATH")"
rm -f "$OUT_PATH"
(cd "$TMP_DIR" && zip -r "$OUT_PATH" . -x "*.DS_Store")
