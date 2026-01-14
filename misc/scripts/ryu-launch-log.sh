#!/bin/bash
set -euo pipefail

# Invoked via `make ryu-launch-log` / `./exlaunch.sh ryu-launch-log`.
# Expects config.mk/Makefile to export relevant variables.

if [[ -z "${PROGRAM_ID:-}" ]]; then
    echo "PROGRAM_ID is not set. Run via make (config.mk exports it)."
    exit 1
fi

if [[ -z "${NAME:-}" ]]; then
    echo "NAME is not set. Run via make (Makefile exports it)."
    exit 1
fi

PY="${PYTHON:-python3}"

# Support two dev envs.
if [[ "${OSTYPE:-}" == "darwin"* ]]; then
    # macOS
    export RYU_PATH="${RYU_MAC_PATH:-}"
    RYU_EXE_DEFAULT="/Applications/Ryujinx.app/Contents/MacOS/Ryujinx"
else
    export RYU_PATH="${RYU_WIN_PATH:-}"
    RYU_EXE_DEFAULT="Ryujinx"
fi

if [[ -z "${RYU_PATH:-}" ]]; then
    echo "RYU_PATH appears to not be set! Check config.mk (RYU_MAC_PATH/RYU_WIN_PATH)."
    exit 1
fi

# Avoid path mismatches in Ryujinx (some code paths treat duplicate slashes as distinct).
export RYU_PATH="${RYU_PATH%/}"

RYU_EXE="${RYU_EXE:-$RYU_EXE_DEFAULT}"
RYU_GAME_PATH="${RYU_GAME_PATH:-}"

if [[ -z "${RYU_GAME_PATH}" ]]; then
    echo "RYU_GAME_PATH is not set (path to .nsp/.xci)."
    echo "Example: RYU_GAME_PATH=\"/path/to/game.nsp\" make ryu-launch-log"
    exit 1
fi

MODS_JSON="${RYU_MODS_JSON:-${RYU_PATH}/games/${PROGRAM_ID}/mods.json}"
MOD_PATH="${RYU_PATH}/mods/contents/${PROGRAM_ID}/${NAME}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
FILTER_LOCAL="${SCRIPT_DIR}/ryu_filter_logs.py"

# Prefer resolving via the *common git dir* so this works from git-worktrees even if the
# worktree itself doesn't have a `.codex/` directory.
D3HACK_MAIN_ROOT="$(
    cd "$(dirname "$(git rev-parse --git-common-dir 2>/dev/null)")" && pwd
)"
FILTER_CODEX="${D3HACK_MAIN_ROOT}/.codex/skills/launch-game-and-log/scripts/ryu_filter_logs.py"

ENABLE_SCRIPT="${SCRIPT_DIR}/ryu_enable_only_mod.py"
if [[ ! -f "$ENABLE_SCRIPT" ]]; then
    echo "Missing enable script: $ENABLE_SCRIPT"
    exit 1
fi

"$PY" "$ENABLE_SCRIPT" \
    --mods-json "$MODS_JSON" \
    --no-backup \
    --program-id "$PROGRAM_ID" \
    --mod-name "$NAME" \
    --mod-path "$MOD_PATH" \
    ${RYU_ENABLE_VERBOSE:+--verbose}

if [[ ! -x "$RYU_EXE" ]] && ! command -v "$RYU_EXE" >/dev/null 2>&1; then
    echo "Ryujinx executable not found: ${RYU_EXE}"
    echo "Set RYU_EXE (or install Ryujinx in PATH)."
    exit 1
fi

echo "Launching Ryujinx..."
echo "Ryujinx: ${RYU_EXE}"
echo "Game: ${RYU_GAME_PATH}"

STATE_DIR="${RYU_STATE_DIR:-/tmp/d3hack_ryu}"
mkdir -p "$STATE_DIR"
PIDFILE="${RYU_PIDFILE:-${STATE_DIR}/ryu_${NAME}.pid}"
export RYU_PIDFILE="$PIDFILE"

FILTER=()
if [[ -f "$FILTER_LOCAL" ]]; then
    FILTER=("$PY" "$FILTER_LOCAL")
elif [[ -f "$FILTER_CODEX" ]]; then
    FILTER=("$PY" "$FILTER_CODEX")
else
    FILTER=("cat")
fi

FIFO="$(mktemp -t ryu_launch_log_fifo.XXXXXX)"
rm -f "$FIFO"
mkfifo "$FIFO"

ryu_pid=""
filter_pid=""
cleanup() {
    rm -f "$FIFO"
    if [[ -n "${ryu_pid:-}" ]]; then
        kill "$ryu_pid" >/dev/null 2>&1 || true
    fi
    if [[ -n "${filter_pid:-}" ]]; then
        kill "$filter_pid" >/dev/null 2>&1 || true
    fi
}
trap cleanup EXIT INT TERM

PYTHONUNBUFFERED=1 "${FILTER[@]}" <"$FIFO" &
filter_pid="$!"

"$RYU_EXE" "$RYU_GAME_PATH" >"$FIFO" 2>&1 &
ryu_pid="$!"
export RYU_PID="$ryu_pid"
printf '%s\n' "$RYU_PID" >"$PIDFILE"
echo "RYU_PID=$RYU_PID"
echo "RYU_PIDFILE=$RYU_PIDFILE"

wait "$ryu_pid"
ryu_status="$?"

filter_status="0"
wait "$filter_pid" || filter_status="$?"

if [[ "$ryu_status" != "0" ]]; then
    exit "$ryu_status"
fi
exit "$filter_status"
