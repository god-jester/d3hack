#!/bin/bash
set -euo pipefail

# Post a key event directly to the Ryujinx process via Quartz (CGEventPostToPid),
# without changing focus. Useful for checking whether Ryujinx will accept events
# while minimized / in Stage Manager.
#
# Usage:
#   misc/scripts/ryu-post-key.sh z
#   RYU_PID=123 misc/scripts/ryu-post-key.sh z
#   RYU_UNMINIMIZE=1 misc/scripts/ryu-post-key.sh z
#
# Notes:
# - Requires macOS.
# - Requires Accessibility permission (same requirement as other Quartz helpers).

if [[ "${OSTYPE:-}" != "darwin"* ]]; then
    echo "ryu-post-key.sh currently supports macOS only."
    exit 1
fi

KEY="${1:-z}"

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

resolve_ryu_pid() {
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
    pid="$(pid_from_pidfile "$pidfile" || true)"
    if [[ -n "$pid" ]]; then
        if is_ryujinx_pid "$pid"; then
            echo "$pid"
            return 0
        fi
        echo "Warning: stale pidfile (no Ryujinx pid=$pid): $pidfile" >&2
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

if ! RYU_PID="$(resolve_ryu_pid)"; then
    exit 1
fi
if [[ -z "$RYU_PID" ]]; then
    echo "Ryujinx is not running (no PID found)."
    exit 1
fi

POST_BIN="${RYU_QUARTZ_POST_KEY_BIN:-/tmp/ryu_quartz_post_key}"
POST_SRC="${RYU_QUARTZ_POST_KEY_SRC:-/tmp/ryu_quartz_post_key.c}"

if [[ ! -x "$POST_BIN" ]]; then
    cat >"$POST_SRC" <<'C'
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CGKeyCode keycode_for_token(const char* tok) {
    if (!tok || !*tok) return (CGKeyCode)UINT16_MAX;

    // Special-cases.
    if (strcmp(tok, "f8") == 0) return (CGKeyCode)kVK_F8;

    // Single-character A-Z / a-z.
    if (tok[1] == '\0') {
        char c = (char)tolower((unsigned char)tok[0]);
        switch (c) {
            case 'a': return (CGKeyCode)kVK_ANSI_A;
            case 'b': return (CGKeyCode)kVK_ANSI_B;
            case 'c': return (CGKeyCode)kVK_ANSI_C;
            case 'd': return (CGKeyCode)kVK_ANSI_D;
            case 'e': return (CGKeyCode)kVK_ANSI_E;
            case 'f': return (CGKeyCode)kVK_ANSI_F;
            case 'g': return (CGKeyCode)kVK_ANSI_G;
            case 'h': return (CGKeyCode)kVK_ANSI_H;
            case 'i': return (CGKeyCode)kVK_ANSI_I;
            case 'j': return (CGKeyCode)kVK_ANSI_J;
            case 'k': return (CGKeyCode)kVK_ANSI_K;
            case 'l': return (CGKeyCode)kVK_ANSI_L;
            case 'm': return (CGKeyCode)kVK_ANSI_M;
            case 'n': return (CGKeyCode)kVK_ANSI_N;
            case 'o': return (CGKeyCode)kVK_ANSI_O;
            case 'p': return (CGKeyCode)kVK_ANSI_P;
            case 'q': return (CGKeyCode)kVK_ANSI_Q;
            case 'r': return (CGKeyCode)kVK_ANSI_R;
            case 's': return (CGKeyCode)kVK_ANSI_S;
            case 't': return (CGKeyCode)kVK_ANSI_T;
            case 'u': return (CGKeyCode)kVK_ANSI_U;
            case 'v': return (CGKeyCode)kVK_ANSI_V;
            case 'w': return (CGKeyCode)kVK_ANSI_W;
            case 'x': return (CGKeyCode)kVK_ANSI_X;
            case 'y': return (CGKeyCode)kVK_ANSI_Y;
            case 'z': return (CGKeyCode)kVK_ANSI_Z;
            default: break;
        }
    }

    // Common named keys.
    if (strcmp(tok, "up") == 0) return (CGKeyCode)kVK_UpArrow;
    if (strcmp(tok, "down") == 0) return (CGKeyCode)kVK_DownArrow;
    if (strcmp(tok, "left") == 0) return (CGKeyCode)kVK_LeftArrow;
    if (strcmp(tok, "right") == 0) return (CGKeyCode)kVK_RightArrow;
    if (strcmp(tok, "space") == 0) return (CGKeyCode)kVK_Space;
    if (strcmp(tok, "enter") == 0) return (CGKeyCode)kVK_Return;
    if (strcmp(tok, "return") == 0) return (CGKeyCode)kVK_Return;
    if (strcmp(tok, "esc") == 0) return (CGKeyCode)kVK_Escape;
    if (strcmp(tok, "escape") == 0) return (CGKeyCode)kVK_Escape;

    return (CGKeyCode)UINT16_MAX;
}

static CGEventFlags flags_for_modifier(const char* tok) {
    if (!tok || !*tok) return 0;
    if (strcmp(tok, "shift") == 0) return kCGEventFlagMaskShift;
    if (strcmp(tok, "ctrl") == 0 || strcmp(tok, "control") == 0) return kCGEventFlagMaskControl;
    if (strcmp(tok, "alt") == 0 || strcmp(tok, "option") == 0) return kCGEventFlagMaskAlternate;
    if (strcmp(tok, "cmd") == 0 || strcmp(tok, "command") == 0) return kCGEventFlagMaskCommand;
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <pid> <key>\n", argv[0]);
        return 2;
    }

    pid_t pid = (pid_t)strtol(argv[1], NULL, 10);
    if (pid <= 0) {
        fprintf(stderr, "Invalid pid: %s\n", argv[1]);
        return 2;
    }

    if (!AXIsProcessTrusted()) {
        fprintf(stderr, "Not trusted for Accessibility. Enable it for your terminal/app.\n");
        return 3;
    }

    // Parse tokens like: "shift+g" or "cmd+shift+f8"
    char buf[128] = {0};
    strncpy(buf, argv[2], sizeof(buf) - 1);

    CGEventFlags flags = 0;
    CGKeyCode key = (CGKeyCode)UINT16_MAX;

    char* saveptr = NULL;
    for (char* tok = strtok_r(buf, "+", &saveptr); tok; tok = strtok_r(NULL, "+", &saveptr)) {
        for (char* p = tok; *p; p++) *p = (char)tolower((unsigned char)*p);
        CGEventFlags f = flags_for_modifier(tok);
        if (f) {
            flags |= f;
            continue;
        }

        // Treat the first non-modifier token as the key.
        if (key == (CGKeyCode)UINT16_MAX) {
            key = keycode_for_token(tok);
        }
    }

    if (key == (CGKeyCode)UINT16_MAX) {
        fprintf(stderr, "Unsupported key token: %s\n", argv[2]);
        return 2;
    }

    CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!src) {
        fprintf(stderr, "Failed to create event source.\n");
        return 4;
    }

    CGEventRef down = CGEventCreateKeyboardEvent(src, key, true);
    CGEventRef up = CGEventCreateKeyboardEvent(src, key, false);
    if (!down || !up) {
        fprintf(stderr, "Failed to create keyboard events.\n");
        if (down) CFRelease(down);
        if (up) CFRelease(up);
        CFRelease(src);
        return 4;
    }

    if (flags) {
        CGEventSetFlags(down, flags);
        CGEventSetFlags(up, flags);
    }

    CGEventPostToPid(pid, down);
    usleep(20000);
    CGEventPostToPid(pid, up);

    CFRelease(down);
    CFRelease(up);
    CFRelease(src);
    return 0;
}
C

    clang -O2 -Wall -Wextra \
        -framework ApplicationServices \
        -framework Carbon \
        -o "$POST_BIN" "$POST_SRC"
fi

if [[ "${RYU_UNMINIMIZE:-0}" == "1" ]]; then
    AX_BIN="${RYU_AX_MINIMIZE_BIN:-/tmp/ryu_ax_set_minimized}"
    AX_SRC="${RYU_AX_MINIMIZE_SRC:-/tmp/ryu_ax_set_minimized.c}"

    if [[ ! -x "$AX_BIN" ]]; then
        # Reuse the same helper as ryu-screenshot.sh (compile lazily).
        cat >"$AX_SRC" <<'C'
#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static int cfbool_to_int(CFTypeRef b, int def) {
    if (!b) return def;
    if (CFGetTypeID(b) != CFBooleanGetTypeID()) return def;
    return CFBooleanGetValue((CFBooleanRef)b) ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <pid> <0|1>\n", argv[0]);
        return 2;
    }

    pid_t pid = (pid_t)strtol(argv[1], NULL, 10);
    int want = atoi(argv[2]) ? 1 : 0;
    if (pid <= 0) return 2;

    if (!AXIsProcessTrusted()) {
        fprintf(stderr, "Not trusted for Accessibility.\n");
        return 3;
    }

    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) return 4;

    CFTypeRef windows = NULL;
    AXError err = AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, &windows);
    if (err != kAXErrorSuccess || !windows || CFGetTypeID(windows) != CFArrayGetTypeID()) {
        if (windows) CFRelease(windows);
        CFRelease(app);
        return 5;
    }

    CFArrayRef arr = (CFArrayRef)windows;
    CFIndex count = CFArrayGetCount(arr);
    int was_minimized = 0;
    int have_state = 0;

    for (CFIndex i = 0; i < count; i++) {
        AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(arr, i);
        if (!win) continue;

        CFTypeRef minimized = NULL;
        if (AXUIElementCopyAttributeValue(win, kAXMinimizedAttribute, &minimized) == kAXErrorSuccess) {
            if (!have_state) {
                was_minimized = cfbool_to_int(minimized, 0);
                have_state = 1;
            }
            if (minimized) CFRelease(minimized);
        }

        AXUIElementSetAttributeValue(
            win,
            kAXMinimizedAttribute,
            want ? kCFBooleanTrue : kCFBooleanFalse
        );
    }

    printf("%d\n", have_state ? was_minimized : 0);

    CFRelease(windows);
    CFRelease(app);
    return 0;
}
C
        clang -O2 -Wall -Wextra \
            -framework ApplicationServices \
            -o "$AX_BIN" "$AX_SRC"
    fi

    was_minimized="$("$AX_BIN" "$RYU_PID" 0 2>/dev/null || echo 0)"
    "$POST_BIN" "$RYU_PID" "$KEY"
    if [[ "$was_minimized" == "1" ]]; then
        "$AX_BIN" "$RYU_PID" 1 >/dev/null 2>&1 || true
    fi
else
    "$POST_BIN" "$RYU_PID" "$KEY"
fi

echo "Posted key '$KEY' to Ryujinx pid=$RYU_PID"
