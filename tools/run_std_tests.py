#!/usr/bin/env python3
"""Runs the `@test` functions of the standard library (roadmap 3.11) and
of the stage 1 compiler in `compiler/` (roadmap phase 4).

Every std module is also checked whole, as the root: other programs check
only the std bodies they use. Every module with tests (in its `_test.of`
file) is compiled with `ofir test`, which
runs each test in a process of its own. Fails if any test fails. Then
checks `ofir test` itself: the standard output of each
`tests/ofir_test/*.of` must match its `.out` file.

    python3 tools/run_std_tests.py --compiler build/stage0/ofir
"""

import argparse
import concurrent.futures
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


# The parts of `compiler/` that compile so far; each port adds its directory
# (roadmap phase 4: base, diag, syntax, ...).
DIRS = ["std", "compiler/base", "compiler/diag", "compiler/syntax", "compiler/sema",
        "compiler/driver"]


def executable(path: str) -> str:
    """The compiler as an absolute path; on Windows, `.exe` may be implied."""
    full = Path(path).resolve()
    if os.name == "nt" and not full.suffix and full.with_suffix(".exe").exists():
        full = full.with_suffix(".exe")
    return str(full)


def modules_with_tests() -> list[Path]:
    """The modules with tests: the module of each `x_test.of` file
    (spec 12.2), and any module with `@test` functions of its own."""
    found = set()
    for path in sorted(p for d in DIRS for p in (ROOT / d).rglob("*.of")):
        if path.stem.endswith("_test"):
            found.add(path.with_name(path.stem[:-len("_test")] + ".of"))
        elif any(line.startswith("@test") for line in path.read_text().splitlines()):
            found.add(path)
    return sorted(found)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", type=executable, required=True)
    parser.add_argument("-j", "--jobs", type=int, default=min(4, os.cpu_count() or 1),
                        help="maximum concurrent compiler processes (default: up to 4)")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be at least 1")
    failed = []
    total = 0
    paths = modules_with_tests()
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = pool.map(lambda path: subprocess.run(
            [args.compiler, "test", str(path), "--deny-warnings"],
            capture_output=True, text=True), paths)
        for path, result in zip(paths, results):
            total += sum(1 for line in result.stdout.splitlines() if line.startswith("test "))
            if result.returncode != 0:
                failed.append(path.relative_to(ROOT))
                sys.stdout.write(result.stdout)
                sys.stderr.write(result.stderr)
        print(f"unit tests (std, compiler): {total} tests in {len(paths)} modules, "
              f"{len(failed)} modules with failures")
        checked = check_std_whole(args.compiler, pool)
    return 1 if failed or not checked or not check_fixtures(args.compiler) else 0


def check_std_whole(compiler: str, pool: concurrent.futures.ThreadPoolExecutor) -> bool:
    """Checks each std module as the root, where every body is checked: a
    program checks only the std bodies it uses (sema_check_late)."""
    ok = True
    paths = [p for p in sorted((ROOT / "std").rglob("*.of")) if not p.stem.endswith("_test")]
    results = pool.map(lambda path: subprocess.run(
        [compiler, "build", str(path), "--stop-after=check", "--deny-warnings"],
        capture_output=True, text=True), paths)
    for path, result in zip(paths, results):
        if result.returncode != 0:
            ok = False
            print(f"FAIL {path.relative_to(ROOT)}: does not check as a root module")
            sys.stderr.write(result.stderr)
    print(f"std modules checked whole: {len(paths)}, {'all' if ok else 'not all'} clean")
    return ok


def check_fixtures(compiler: str) -> bool:
    ok = True
    for path in sorted((ROOT / "tests" / "ofir_test").glob("*.of")):
        if path.stem.endswith("_test"):
            continue   # loaded with its module
        result = subprocess.run([compiler, "test", str(path)], capture_output=True, text=True)
        expected = path.with_suffix(".out").read_text()
        if result.stdout != expected:
            ok = False
            print(f"FAIL {path.relative_to(ROOT)}: the output of `ofir test` differs")
            sys.stdout.write(result.stdout)
    return ok


if __name__ == "__main__":
    sys.exit(main())
