#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import posixpath
import sys
from datetime import datetime
from pathlib import Path


def _norm_path(value: str) -> str:
    # Ryujinx sometimes serializes paths with platform-specific separators.
    # Normalize conservatively without requiring the path to exist.
    p = value.strip().replace("\\", "/").rstrip("/")
    p = posixpath.normpath(p)
    if p == ".":
        p = ""
    # Normalize case on Windows-style paths; harmless elsewhere.
    return os.path.normcase(p)


def _expected_suffix(program_id: str, mod_name: str) -> str:
    pid = program_id.lower()
    return _norm_path(f"/mods/contents/{pid}/{mod_name}")


def _load_json(path: Path) -> dict:
    if not path.exists():
        return {"mods": []}
    raw = path.read_text(encoding="utf-8")
    if not raw.strip():
        return {"mods": []}
    data = json.loads(raw)
    if not isinstance(data, dict):
        raise TypeError(f"Expected top-level JSON object, got {type(data).__name__}")
    return data


def _backup(path: Path) -> Path:
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup = path.with_suffix(path.suffix + f".bak.{ts}")
    backup.write_text(path.read_text(encoding="utf-8"), encoding="utf-8")
    return backup


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Enable only the specified mod in Ryujinx's mods.json, disabling all others."
    )
    parser.add_argument("--mods-json", type=Path, required=True)
    parser.add_argument("--program-id", required=True)
    parser.add_argument("--mod-name", required=True)
    parser.add_argument("--mod-path", required=True)
    parser.add_argument("--no-backup", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()

    mods_json = Path(args.mods_json).expanduser()
    program_id = str(args.program_id).lower()
    mod_name = str(args.mod_name)
    mod_path = _norm_path(str(args.mod_path))

    mods_json.parent.mkdir(parents=True, exist_ok=True)

    data = _load_json(mods_json)
    mods_key = None
    if isinstance(data, dict):
        for k in data.keys():
            if isinstance(k, str) and k.lower() == "mods":
                mods_key = k
                break
    mods = data.get(mods_key or "mods") if isinstance(data, dict) else None
    if not isinstance(mods, list):
        mods = []

    target_norm = _norm_path(mod_path)
    suffix_norm = _expected_suffix(program_id, mod_name)

    def disable_enabled_flags(obj: object) -> None:
        if not isinstance(obj, dict):
            return
        for k in list(obj.keys()):
            if not isinstance(k, str):
                continue
            lk = k.lower()
            if lk in {"enabled", "isenabled", "is_enabled"}:
                obj[k] = False
        for k, v in list(obj.items()):
            if isinstance(v, dict):
                disable_enabled_flags(v)
            elif isinstance(v, list):
                for item in v:
                    disable_enabled_flags(item)

    def enable_entry(entry: dict) -> None:
        entry["name"] = mod_name
        entry["path"] = mod_path
        entry["enabled"] = True

    updated: list[object] = []

    def is_target(entry: dict) -> bool:
        entry_path = entry.get("path")
        entry_name = entry.get("name")
        if isinstance(entry_path, str):
            p_norm = _norm_path(entry_path)
            if p_norm == target_norm:
                return True
            if p_norm.endswith(suffix_norm):
                return True
        if isinstance(entry_name, str) and entry_name == mod_name:
            return True
        return False

    # First pass: normalize + aggressively disable anything that looks like an enable flag.
    for entry in mods:
        if isinstance(entry, dict):
            entry_path = entry.get("path")
            if isinstance(entry_path, str) and entry_path:
                entry["path"] = _norm_path(entry_path)
            entry["enabled"] = False
            disable_enabled_flags(entry)

    found = False
    for entry in mods:
        if not isinstance(entry, dict):
            updated.append(entry)
            continue

        if is_target(entry):
            found = True
            enable_entry(entry)

        updated.append(entry)

    if not found:
        updated.append({"name": mod_name, "path": mod_path, "enabled": True})

    if args.verbose:
        enabled = []
        for entry in updated:
            if isinstance(entry, dict) and entry.get("enabled") is True:
                enabled.append((entry.get("name"), entry.get("path")))
        print(f"Enabled entries after update: {enabled}")

    if isinstance(data, dict):
        data[mods_key or "mods"] = updated

    print(f"mods.json: {mods_json}")
    print(f"Enable only: {mod_name}")
    print(f"Mod path: {mod_path}")

    if args.dry_run:
        print("[dry-run] Not writing changes.")
        return 0

    if mods_json.exists() and not args.no_backup:
        backup = _backup(mods_json)
        print(f"Backup: {backup}")

    mods_json.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print("Updated mods.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
