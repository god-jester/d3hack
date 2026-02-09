#!/usr/bin/env python3

import os
import subprocess
import sys
from datetime import datetime


def _run(cmd):
    try:
        out = subprocess.check_output(cmd, stderr=subprocess.STDOUT)
        return out.decode("utf-8", errors="replace").strip()
    except Exception:
        return ""


def _c_escape(s):
    # Minimal C string literal escaping.
    return (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def main():
    if len(sys.argv) != 2:
        print("usage: gen_build_git.py <out_header>", file=sys.stderr)
        return 2

    out_path = sys.argv[1]

    git_describe = _run(["git", "describe", "--tags", "--always", "--dirty"])
    git_commit = _run(["git", "rev-parse", "--short", "HEAD"])
    dirty = 1 if git_describe.endswith("-dirty") else 0
    build_timestamp = datetime.now().strftime("%Y-%m-%d %H:%M")

    if not git_describe:
        git_describe = "unknown"
    if not git_commit:
        git_commit = "unknown"

    text = "\n".join(
        [
            "#pragma once",
            "",
            '#define D3HACK_GIT_DESCRIBE "%s"' % _c_escape(git_describe),
            '#define D3HACK_GIT_COMMIT "%s"' % _c_escape(git_commit),
            "#define D3HACK_GIT_DIRTY %d" % int(dirty),
            '#define D3HACK_BUILD_TIMESTAMP "%s"' % _c_escape(build_timestamp),
            "",
        ]
    )

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
