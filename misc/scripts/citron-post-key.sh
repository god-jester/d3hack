#!/bin/bash
set -euo pipefail

# Like `ryu-post-key.sh`, but targets the Citron emulator process (./citron) instead of Ryujinx.
#
# Usage:
#   misc/scripts/citron-post-key.sh g
#   CITRON_PID=123 misc/scripts/citron-post-key.sh g

if [[ "${OSTYPE:-}" != "darwin"* ]]; then
    echo "citron-post-key.sh currently supports macOS only."
    exit 1
fi

KEY="${1:-g}"

CITRON_PID="${CITRON_PID:-}"
if [[ -z "$CITRON_PID" ]]; then
    CITRON_PID="$(pgrep -x citron || true)"
fi
if [[ -z "$CITRON_PID" ]]; then
    CITRON_PID="$(pgrep -f "/citron(\\s|$)" || true)"
fi
if [[ -z "$CITRON_PID" ]]; then
    echo "Citron is not running (no PID found)."
    exit 1
fi

POST_BIN="${CITRON_QUARTZ_POST_KEY_BIN:-/tmp/citron_quartz_post_key}"
POST_SRC="${CITRON_QUARTZ_POST_KEY_SRC:-/tmp/citron_quartz_post_key.c}"

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

    if (strcmp(tok, "f8") == 0) return (CGKeyCode)kVK_F8;

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

"$POST_BIN" "$CITRON_PID" "$KEY"
echo "Posted key '$KEY' to Citron pid=$CITRON_PID"

