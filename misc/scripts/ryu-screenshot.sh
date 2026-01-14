#!/bin/bash
set -euo pipefail

# Trigger a Ryujinx screenshot hotkey via `ryu-post-key.sh`, then move/copy the resulting
# PNG out of the Ryujinx screenshots directory into /tmp (or a user-provided path).
#
# Ryujinx hotkeys appear to require the Ryujinx window to be active, so this script
# intentionally steals focus briefly, triggers the hotkey, then restores the previous
# frontmost app.

if [[ "${OSTYPE:-}" != "darwin"* ]]; then
    echo "ryu-screenshot.sh currently supports macOS only."
    exit 1
fi

SCREENSHOTS_DIR="${RYU_SCREENSHOTS_DIR:-$HOME/Library/Application Support/Ryujinx/screenshots}"
if [[ ! -d "$SCREENSHOTS_DIR" ]]; then
    echo "Ryujinx screenshots directory not found: $SCREENSHOTS_DIR"
    exit 1
fi

CONFIG_JSON="${RYU_CONFIG_JSON:-$HOME/Library/Application Support/Ryujinx/Config.json}"
HOTKEY="${RYU_SCREENSHOT_KEY:-}"
if [[ -z "$HOTKEY" && -f "$CONFIG_JSON" ]]; then
    HOTKEY="$(python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); print((d.get("hotkeys") or {}).get("screenshot",""))' "$CONFIG_JSON" 2>/dev/null || true)"
fi
HOTKEY="${HOTKEY:-F8}"

OUT="${RYU_SCREENSHOT_OUT:-/tmp/ryu_$(date +%Y%m%d_%H%M%S).png}"
KEEP_ORIGINAL="${RYU_SCREENSHOT_KEEP:-0}"
TIMEOUT_S="${RYU_SCREENSHOT_TIMEOUT_S:-5}"
DEBUG="${RYU_SCREENSHOT_DEBUG:-0}"
RETRIES="${RYU_SCREENSHOT_RETRIES:-2}"
FOCUS_DELAY_S="${RYU_SCREENSHOT_FOCUS_DELAY_S:-0.05}"
FAST_TIMEOUT_S="${RYU_SCREENSHOT_FAST_TIMEOUT_S:-0.75}"
FAST_FOCUS_DELAY_S="${RYU_SCREENSHOT_FAST_FOCUS_DELAY_S:-0.02}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
POST_KEY="${SCRIPT_DIR}/ryu-post-key.sh"
if [[ ! -x "$POST_KEY" ]]; then
    echo "Missing script (or not executable): $POST_KEY"
    exit 1
fi

default_mod_name() {
    if [[ -n "${NAME:-}" ]]; then
        echo "$NAME"
        return 0
    fi

    local top
    top="$(git rev-parse --show-toplevel 2>/dev/null || true)"
    if [[ -n "$top" ]]; then
        basename "$top"
        return 0
    fi

    basename "$PWD"
}

default_pidfile() {
    local state_dir="${RYU_STATE_DIR:-/tmp/d3hack_ryu}"
    local name
    name="$(default_mod_name)"
    echo "${RYU_PIDFILE:-${state_dir}/ryu_${name}.pid}"
}

pid_from_pidfile() {
    local pidfile="$1"
    if [[ -z "$pidfile" || ! -f "$pidfile" ]]; then
        return 1
    fi
    local pid
    pid="$(head -n 1 "$pidfile" 2>/dev/null | tr -d ' \t\r\n' || true)"
    if [[ "$pid" =~ ^[0-9]+$ ]]; then
        echo "$pid"
        return 0
    fi
    return 1
}

is_ryujinx_pid() {
    local pid="$1"
    if [[ -z "$pid" ]]; then
        return 1
    fi
    ps -p "$pid" -o args= 2>/dev/null | grep -Eq 'Ryujinx(\.app/Contents/MacOS/Ryujinx)?([[:space:]]|$)'
}

ryu_pid() {
    local pid="${RYU_PID:-}"
    if [[ -n "$pid" ]]; then
        if ! is_ryujinx_pid "$pid"; then
            echo "RYU_PID is set but does not look like Ryujinx pid=$pid" >&2
            return 1
        fi
        echo "$pid"
        return 0
    fi

    local pidfile
    pidfile="$(default_pidfile)"
    if [[ -n "$pidfile" ]]; then
        pid="$(pid_from_pidfile "$pidfile" || true)"
        if [[ -n "$pid" ]]; then
            if is_ryujinx_pid "$pid"; then
                echo "$pid"
                return 0
            fi
            echo "Warning: stale pidfile (no Ryujinx pid=$pid): $pidfile" >&2
        fi
    fi

    local pids=()
    while IFS= read -r line; do
        [[ -n "$line" ]] && pids+=("$line")
    done < <(pgrep -x Ryujinx || true)
    if (( ${#pids[@]} == 0 )); then
        while IFS= read -r line; do
            [[ -n "$line" ]] && pids+=("$line")
        done < <(pgrep -f "Ryujinx.app/Contents/MacOS/Ryujinx" || true)
    fi

    if (( ${#pids[@]} == 0 )); then
        return 0
    fi

    if (( ${#pids[@]} == 1 )); then
        echo "${pids[0]}"
        return 0
    fi

    echo "Multiple Ryujinx instances detected: ${pids[*]}" >&2
    echo "Set RYU_PID=<pid>, or run ryu-launch-log to write a pidfile for this worktree (default: $pidfile)." >&2
    return 1
}

frontmost_pid() {
    # Prefer Accessibility's "focused application" (reliable under Stage Manager),
    # then fall back to CGWindowList heuristics.
    local ax_bin="${RYU_AX_FRONT_PID_BIN:-/tmp/ryu_ax_frontmost_pid}"
    local ax_src="${RYU_AX_FRONT_PID_SRC:-/tmp/ryu_ax_frontmost_pid.c}"

    if [[ ! -x "$ax_bin" ]]; then
        cat >"$ax_src" <<'C'
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    if (!AXIsProcessTrusted()) return 3;

    AXUIElementRef sys = AXUIElementCreateSystemWide();
    if (!sys) return 4;

    CFTypeRef app = NULL;
    AXError err = AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, &app);
    if (err != kAXErrorSuccess || !app) {
        CFRelease(sys);
        return 5;
    }

    pid_t pid = 0;
    if (AXUIElementGetPid((AXUIElementRef)app, &pid) != kAXErrorSuccess || pid <= 0) {
        CFRelease(app);
        CFRelease(sys);
        return 6;
    }

    printf("%d\n", pid);
    CFRelease(app);
    CFRelease(sys);
    return 0;
}
C
        clang -O2 -Wall -Wextra \
            -framework ApplicationServices \
            -o "$ax_bin" "$ax_src"
    fi

    local pid
    pid="$("$ax_bin" 2>/dev/null || true)"
    if [[ -n "$pid" ]]; then
        echo "$pid"
        return 0
    fi

    local cg_bin="${RYU_QUARTZ_FRONT_PID_BIN:-/tmp/ryu_quartz_frontmost_pid}"
    local cg_src="${RYU_QUARTZ_FRONT_PID_SRC:-/tmp/ryu_quartz_frontmost_pid.c}"
    if [[ ! -x "$cg_bin" ]]; then
        cat >"$cg_src" <<'C'
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

static long cfnum_to_long(CFNumberRef n, long def) {
    if (!n) return def;
    long v = def;
    if (CFNumberGetValue(n, kCFNumberLongType, &v)) return v;
    return def;
}

int main(void) {
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly, kCGNullWindowID);
    if (!list) return 2;
    CFIndex count = CFArrayGetCount(list);
    for (CFIndex i = 0; i < count; i++) {
        CFDictionaryRef d = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
        if (!d) continue;
        long layer = cfnum_to_long(CFDictionaryGetValue(d, kCGWindowLayer), 0);
        if (layer != 0) continue;
        long pid = cfnum_to_long(CFDictionaryGetValue(d, kCGWindowOwnerPID), -1);
        if (pid > 0) {
            printf("%ld\n", pid);
            CFRelease(list);
            return 0;
        }
    }
    CFRelease(list);
    return 3;
}
C
        clang -O2 -Wall -Wextra \
            -framework ApplicationServices \
            -o "$cg_bin" "$cg_src"
    fi

    "$cg_bin" 2>/dev/null || true
}

ax_focus_pid() {
    local target_pid="$1"
    if [[ -z "$target_pid" ]]; then
        return 1
    fi
    local ax_bin="${RYU_AX_FOCUS_BIN:-/tmp/ryu_ax_focus_pid}"
    local ax_src="${RYU_AX_FOCUS_SRC:-/tmp/ryu_ax_focus_pid.c}"

    if [[ ! -x "$ax_bin" ]]; then
        cat >"$ax_src" <<'C'
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static void set_bool_attr(AXUIElementRef el, CFStringRef attr, int v) {
    AXUIElementSetAttributeValue(el, attr, v ? kCFBooleanTrue : kCFBooleanFalse);
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    pid_t pid = (pid_t)strtol(argv[1], NULL, 10);
    if (pid <= 0) return 2;

    if (!AXIsProcessTrusted()) return 3;

    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return 4;

    set_bool_attr(app, kAXFrontmostAttribute, 1);

    CFTypeRef windows = NULL;
    AXError err = AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, &windows);
    if (err == kAXErrorSuccess && windows && CFGetTypeID(windows) == CFArrayGetTypeID()) {
        CFArrayRef arr = (CFArrayRef)windows;
        CFIndex count = CFArrayGetCount(arr);
        for (CFIndex i = 0; i < count; i++) {
            AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(arr, i);
            if (!win) continue;
            AXUIElementSetAttributeValue(win, kAXMinimizedAttribute, kCFBooleanFalse);
            AXUIElementPerformAction(win, kAXRaiseAction);
            if (i == 0) {
                set_bool_attr(win, kAXMainAttribute, 1);
                set_bool_attr(win, kAXFocusedAttribute, 1);
            }
        }
    }

    if (windows) CFRelease(windows);
    CFRelease(app);
    return 0;
}
C
        clang -O2 -Wall -Wextra \
            -framework ApplicationServices \
            -o "$ax_bin" "$ax_src"
    fi

    "$ax_bin" "$target_pid" >/dev/null 2>&1 || true
}

wait_for_png_newer_than() {
    local marker="$1"
    local timeout="$2"
    if [[ "$timeout" == *.* ]]; then
        local end
        end="$(python3 -c 'import sys,time; print(time.time()+float(sys.argv[1]))' "$timeout" 2>/dev/null || true)"
        if [[ -z "$end" ]]; then
            return 1
        fi
        while python3 -c 'import sys,time; sys.exit(0 if time.time() < float(sys.argv[1]) else 1)' "$end" >/dev/null 2>&1; do
            local newest
            newest="$(find "$SCREENSHOTS_DIR" -maxdepth 1 -type f -name '*.png' -newer "$marker" -print0 2>/dev/null | xargs -0 ls -t 2>/dev/null | head -n 1 || true)"
            if [[ -n "$newest" ]]; then
                echo "$newest"
                return 0
            fi
            sleep 0.05
        done
        return 1
    fi

    local deadline=$((SECONDS + timeout))
    while (( SECONDS < deadline )); do
        local newest
        newest="$(find "$SCREENSHOTS_DIR" -maxdepth 1 -type f -name '*.png' -newer "$marker" -print0 2>/dev/null | xargs -0 ls -t 2>/dev/null | head -n 1 || true)"
        if [[ -n "$newest" ]]; then
            echo "$newest"
            return 0
        fi
        sleep 0.1
    done
    return 1
}

latest_png() {
    ls -t "$SCREENSHOTS_DIR"/*.png 2>/dev/null | head -n 1 || true
}

logd() {
    if [[ "$DEBUG" == "1" ]]; then
        echo "[ryu-screenshot] $*" >&2
    fi
}

unique_out_path() {
    local base="$1"
    if [[ ! -e "$base" ]]; then
        echo "$base"
        return 0
    fi
    local ext="${base##*.}"
    local stem="${base%.*}"
    local i=1
    while [[ -e "${stem}_${i}.${ext}" ]]; do
        i=$((i + 1))
    done
    echo "${stem}_${i}.${ext}"
}

if ! pid="$(ryu_pid)"; then
    exit 1
fi
if [[ -z "$pid" ]]; then
    echo "Ryujinx is not running (no PID found)."
    exit 1
fi

prev_pid="$(frontmost_pid)"
logd "prev_pid=${prev_pid:-<empty>}"
logd "ryu_pid=$pid"
newest=""
attempt=0
while (( attempt <= RETRIES )); do
    attempt=$((attempt + 1))
    logd "attempt=$attempt hotkey=$HOTKEY"

    ax_focus_pid "$pid"
    if [[ "$attempt" == "1" ]]; then
        sleep "$FAST_FOCUS_DELAY_S"
    else
        sleep "$FOCUS_DELAY_S"
    fi

    MARKER="$(mktemp -t ryu_screenshot_marker.XXXXXX)"
    trap 'rm -f "$MARKER"' EXIT
    sleep 0.05

    before="$(latest_png)"
    logd "before=${before:-<none>}"

    RYU_PID="$pid" "$POST_KEY" "$HOTKEY" >/dev/null || true

    if [[ "${RYU_RESTORE_FOCUS:-1}" == "1" && -n "$prev_pid" ]]; then
        sleep 0.05
        ax_focus_pid "$prev_pid"
        logd "restored focus to pid=$prev_pid"
    fi

    if [[ "$attempt" == "1" ]]; then
        newest="$(wait_for_png_newer_than "$MARKER" "$FAST_TIMEOUT_S" || true)"
    else
        newest="$(wait_for_png_newer_than "$MARKER" "$TIMEOUT_S" || true)"
    fi
    if [[ -z "$newest" ]]; then
        after="$(latest_png)"
        logd "marker wait missed; after=${after:-<none>}"
        if [[ -n "$after" && "$after" != "$before" ]]; then
            newest="$after"
        fi
    fi

    if [[ -n "$newest" ]]; then
        break
    fi
done

if [[ -n "$newest" ]]; then
    out_path="$(unique_out_path "$OUT")"
    if [[ "$KEEP_ORIGINAL" == "1" ]]; then
        cp -f "$newest" "$out_path"
    else
        mv -f "$newest" "$out_path"
    fi
    echo "$out_path"
    exit 0
fi

echo "Timed out waiting for Ryujinx screenshot after hotkey '$HOTKEY' in: $SCREENSHOTS_DIR (attempts=$attempt)."
echo "Hint: retry, or bump RYU_SCREENSHOT_RETRIES / RYU_SCREENSHOT_TIMEOUT_S / RYU_SCREENSHOT_FOCUS_DELAY_S, or set RYU_SCREENSHOT_KEY explicitly."
exit 1
