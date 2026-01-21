#!/usr/bin/env python
"""Parallel find-all-symbols runner (Python 3 compatible)."""

import argparse
import fnmatch
import json
import multiprocessing
import os
import queue
import shutil
import subprocess
import sys
import tempfile
import threading


def find_compilation_database(db_name):
    """Adjusts the directory until a compilation database is found."""
    result = "."
    while not os.path.isfile(os.path.join(result, db_name)):
        if os.path.realpath(result) == "/":
            print("Error: could not find compilation database.")
            sys.exit(1)
        result = os.path.join(result, "..")
    return os.path.realpath(result)


def merge_symbols(directory, args):
    """Merge all symbol files (yaml) in a given directory into a single file."""
    invocation = [args.binary, "-merge-dir=" + directory, args.saving_path]
    subprocess.check_call(invocation)
    print("Merge is finished. Saving results in " + args.saving_path)


def run_find_all_symbols(args, tmpdir, build_path, work_queue):
    """Takes filenames out of queue and runs find-all-symbols on them."""
    while True:
        name = work_queue.get()
        invocation = [
            args.binary,
            name,
            "-output-dir=" + tmpdir,
            "-p=" + build_path,
        ]
        for arg in args.extra_arg_before:
            invocation.append("--extra-arg-before=" + arg)
        for arg in args.extra_arg:
            invocation.append("--extra-arg=" + arg)
        sys.stdout.write(" ".join(invocation) + "\n")
        subprocess.call(invocation)
        work_queue.task_done()


def is_source_file(path):
    ext = os.path.splitext(path)[1].lower()
    return ext in {".c", ".cc", ".cpp", ".cxx", ".m", ".mm"}


def resolve_binary(binary_arg):
    if binary_arg:
        return binary_arg
    return shutil.which("find-all-symbols") or "./bin/find-all-symbols"


def main():
    parser = argparse.ArgumentParser(
        description="Runs find-all-symbols over all files in a compilation database."
    )
    parser.add_argument(
        "-binary",
        metavar="PATH",
        default=None,
        help="path to find-all-symbols binary",
    )
    parser.add_argument(
        "-j",
        type=int,
        default=0,
        help="number of instances to be run in parallel.",
    )
    parser.add_argument(
        "-p",
        dest="build_path",
        help="path used to read a compilation database.",
    )
    parser.add_argument(
        "--only-under",
        default=None,
        help="only process files under this directory (path prefix match).",
    )
    parser.add_argument(
        "--exclude",
        action="append",
        default=[],
        help="exclude files matching this glob (may be repeated).",
    )
    parser.add_argument(
        "--extra-arg",
        action="append",
        default=[],
        help="additional argument to append to the compiler command line.",
    )
    parser.add_argument(
        "--extra-arg-before",
        action="append",
        default=[],
        help="additional argument to prepend to the compiler command line.",
    )
    parser.add_argument(
        "-saving-path",
        default="./find_all_symbols_db.yaml",
        help="result saving path",
    )
    args = parser.parse_args()

    args.binary = resolve_binary(args.binary)
    if shutil.which(args.binary) is None and not os.path.isfile(args.binary):
        print("Error: find-all-symbols binary not found. Use -binary to set it.")
        sys.exit(1)

    db_path = "compile_commands.json"

    if args.build_path is not None:
        build_path = args.build_path
    else:
        build_path = find_compilation_database(db_path)

    tmpdir = tempfile.mkdtemp()

    database = json.load(open(os.path.join(build_path, db_path)))
    files = [entry["file"] for entry in database if is_source_file(entry.get("file", ""))]
    files = [f for f in files if not f.endswith(".rc")]
    if args.only_under:
        root = os.path.realpath(args.only_under)
        root_prefix = root + os.sep
        files = [f for f in files if os.path.realpath(f).startswith(root_prefix)]
    if args.exclude:
        filtered = []
        for f in files:
            path = os.path.realpath(f)
            if any(fnmatch.fnmatch(path, pat) for pat in args.exclude):
                continue
            filtered.append(f)
        files = filtered

    max_task = args.j
    if max_task == 0:
        max_task = multiprocessing.cpu_count()

    try:
        work_queue = queue.Queue(max_task)
        for _ in range(max_task):
            thread = threading.Thread(
                target=run_find_all_symbols,
                args=(args, tmpdir, build_path, work_queue),
            )
            thread.daemon = True
            thread.start()

        for name in files:
            work_queue.put(name)

        work_queue.join()
        merge_symbols(tmpdir, args)
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    main()
