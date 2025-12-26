#!/usr/bin/env python3
"""
Import dumped Challenge Rift blobs into the expected rift_data layout.

Usage:
  python3 tools/import_challenge_dumps.py \
      --src /path/to/dumps \
      --dst /path/to/rift_data \
      [--config /path/to/dmp_config.dat] \
      [--rifts /path/to/dmp_*.dat] \
      [--copy] [--dry-run]

Notes:
- If --config is omitted, the smallest file in --src is assumed to be the
  weekly config and written as challengerift_config.dat.
- If --rifts is omitted, all remaining *.dat in --src are sorted by mtime and
  written as challengerift_0.dat, challengerift_1.dat, ...
- By default files are moved; use --copy to copy instead.
"""

from __future__ import annotations

import argparse
import glob
import os
import shutil
from pathlib import Path


def pick_smallest_file(files: list[Path]) -> Path | None:
    if not files:
        return None
    return min(files, key=lambda p: p.stat().st_size)


def sorted_by_mtime(files: list[Path]) -> list[Path]:
    return sorted(files, key=lambda p: p.stat().st_mtime)


def main() -> int:
    ap = argparse.ArgumentParser(description="Import Challenge Rift dump files")
    ap.add_argument("--src", required=True, help="Directory with dumped *.dat files")
    ap.add_argument("--dst", required=True, help="Target rift_data directory")
    ap.add_argument("--config", help="Explicit dump file to use as challengerift_config.dat")
    ap.add_argument(
        "--rifts", nargs="*", help="Explicit list (or glob) of dump files to map to challengerift_N.dat"
    )
    ap.add_argument("--copy", action="store_true", help="Copy instead of move")
    ap.add_argument("--dry-run", action="store_true", help="Show planned actions only")
    args = ap.parse_args()

    src = Path(args.src).expanduser().resolve()
    dst = Path(args.dst).expanduser().resolve()
    dst.mkdir(parents=True, exist_ok=True)

    if not src.is_dir():
        raise SystemExit(f"--src is not a directory: {src}")

    # Collect candidate files
    candidates = [p for p in src.glob("*.dat") if p.is_file()]
    if not candidates:
        raise SystemExit(f"No *.dat files found in {src}")

    # Determine config file
    config_path: Path | None
    if args.config:
        config_path = Path(args.config)
        if not config_path.is_file():
            raise SystemExit(f"--config not found: {config_path}")
    else:
        config_path = pick_smallest_file(candidates)
        if config_path is None:
            raise SystemExit("Could not determine config file (no candidates)")

    # Determine rift files
    if args.rifts:
        rift_files: list[Path] = []
        for pat in args.rifts:
            # support globs and explicit paths
            matches = [Path(p) for p in glob.glob(pat)] if any(ch in pat for ch in "*?[]") else [Path(pat)]
            rift_files.extend([m for m in matches if m.is_file()])
    else:
        rift_files = [p for p in candidates if p != config_path]
        rift_files = sorted_by_mtime(rift_files)

    actions: list[tuple[Path, Path]] = []
    # Map config
    actions.append((config_path, dst / "challengerift_config.dat"))
    # Map rifts sequentially
    for idx, rf in enumerate(rift_files):
        actions.append((rf, dst / f"challengerift_{idx}.dat"))

    # Plan
    verb = "COPY" if args.copy else "MOVE"
    print("Planned actions:")
    for src_path, dst_path in actions:
        print(f"  {verb}: {src_path} -> {dst_path}")

    if args.dry_run:
        return 0

    # Execute
    for src_path, dst_path in actions:
        dst_path.parent.mkdir(parents=True, exist_ok=True)
        if dst_path.exists():
            backup = dst_path.with_suffix(dst_path.suffix + ".bak")
            print(f"  BACKUP: {dst_path} -> {backup}")
            shutil.move(str(dst_path), str(backup))
        if args.copy:
            shutil.copy2(str(src_path), str(dst_path))
        else:
            shutil.move(str(src_path), str(dst_path))

    print("Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

