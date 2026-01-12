#!/bin/bash
set -euo pipefail

# Trigger a Ryujinx screenshot hotkey via `ryu-post-key.sh`, then move/copy the resulting
# PNG out of the Ryujinx screenshots directory into /tmp (or a user-provided path).
#
# This intentionally avoids any focus/AX/window-capture fallbacks: it's meant to stay lean
# and rely on CGEventPostToPid being sufficient. In practice, Ryujinx hotkeys appear to
# only trigger when the Ryujinx window is active; so we provide a minimal opt-out
# focus-steal+restore fallback.

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
    HOTKEY="$(python -c 'import json,sys; d=json.load(open(sys.argv[1])); print((d.get("hotkeys") or {}).get("screenshot",""))' "$CONFIG_JSON" 2>/dev/null || true)"
fi
HOTKEY="${HOTKEY:-F8}"

OUT="${RYU_SCREENSHOT_OUT:-/tmp/ryu_$(date +%Y%m%d_%H%M%S).png}"
KEEP_ORIGINAL="${RYU_SCREENSHOT_KEEP:-0}"
TIMEOUT_S="${RYU_SCREENSHOT_TIMEOUT_S:-5}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
POST_KEY="${SCRIPT_DIR}/ryu-post-key.sh"
if [[ ! -x "$POST_KEY" ]]; then
    echo "Missing script (or not executable): $POST_KEY"
    exit 1
fi

ryu_pid() {
    local pid="${RYU_PID:-}"
    if [[ -z "$pid" ]]; then
        pid="$(pgrep -x Ryujinx || true)"
    fi
    if [[ -z "$pid" ]]; then
        pid="$(pgrep -f "Ryujinx.app/Contents/MacOS/Ryujinx" || true)"
    fi
    echo "$pid"
}

frontmost_pid() {
    local pid_bin="${RYU_QUARTZ_FRONT_PID_BIN:-/tmp/ryu_quartz_frontmost_pid}"
    local pid_src="${RYU_QUARTZ_FRONT_PID_SRC:-/tmp/ryu_quartz_frontmost_pid.c}"

    if [[ ! -x "$pid_bin" ]]; then
        cat >"$pid_src" <<'C'
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
            -o "$pid_bin" "$pid_src"
    fi

    "$pid_bin" 2>/dev/null || true
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

MARKER="$(mktemp -t ryu_screenshot_marker.XXXXXX)"
trap 'rm -f "$MARKER"' EXIT
sleep 0.05

"$POST_KEY" "$HOTKEY" >/dev/null

newest="$(wait_for_png_newer_than "$MARKER" "$TIMEOUT_S" || true)"
if [[ -z "$newest" && "${RYU_SCREENSHOT_FOCUS:-1}" == "1" ]]; then
    prev_pid="$(frontmost_pid)"
    ax_focus_pid "$(ryu_pid)"
    MARKER2="$(mktemp -t ryu_screenshot_marker.XXXXXX)"
    trap 'rm -f "$MARKER" "$MARKER2"' EXIT
    sleep 0.05
    "$POST_KEY" "$HOTKEY" >/dev/null
    newest="$(wait_for_png_newer_than "$MARKER2" "$TIMEOUT_S" || true)"
    if [[ -n "$prev_pid" ]]; then
        ax_focus_pid "$prev_pid"
    fi
fi

if [[ -n "$newest" ]]; then
    if [[ "$KEEP_ORIGINAL" == "1" ]]; then
        cp -f "$newest" "$OUT"
    else
        mv -f "$newest" "$OUT"
    fi
    echo "$OUT"
    exit 0
fi

echo "Timed out waiting for Ryujinx screenshot after hotkey '$HOTKEY' in: $SCREENSHOTS_DIR"
exit 1
