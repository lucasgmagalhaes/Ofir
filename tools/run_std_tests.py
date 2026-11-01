#!/usr/bin/env python3
"""Runs the `@test` functions of the standard library (roadmap 3.11) and
of the stage 1 compiler in `compiler/` (roadmap phase 4).

Every std module is also checked whole, as the root: other programs check
only the std bodies they use. Every module with tests (in its `_test.at`
file) is compiled with `astra test`, which
runs each test in a process of its own. Fails if any test fails. Then
checks `astra test` itself: the standard output of each
`tests/astra_test/*.at` must match its `.out` file.

    python3 tools/run_std_tests.py --compiler build/stage0/astra
"""

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


# The parts of `compiler/` that compile so far; each port adds its directory
# (roadmap phase 4: base, diag, syntax, ...).
DIRS = ["std", "compiler/base", "compiler/diag", "compiler/syntax", "compiler/sema",
        "compiler/driver"]


def modules_with_tests() -> list[Path]:
    """The modules with tests: the module of each `x_test.at` file
    (spec 12.2), and any module with `@test` functions of its own."""
    found = set()
    for path in sorted(p for d in DIRS for p in (ROOT / d).rglob("*.at")):
        if path.stem.endswith("_test"):
            found.add(path.with_name(path.stem[:-len("_test")] + ".at"))
        elif any(line.startswith("@test") for line in path.read_text().splitlines()):
            found.add(path)
    return sorted(found)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    failed = []
    total = 0
    for path in modules_with_tests():
        result = subprocess.run([args.compiler, "test", str(path), "--deny-warnings"],
                                capture_output=True, text=True)
        total += sum(1 for line in result.stdout.splitlines() if line.startswith("test "))
        if result.returncode != 0:
            failed.append(path.relative_to(ROOT))
            sys.stdout.write(result.stdout)
            sys.stderr.write(result.stderr)
    modules = len(modules_with_tests())
    print(f"unit tests (std, compiler): {total} tests in {modules} modules, "
          f"{len(failed)} modules with failures")
    checked = check_std_whole(args.compiler)
    return 1 if failed or not checked or not check_fixtures(args.compiler) else 0


def check_std_whole(compiler: str) -> bool:
    """Checks each std module as the root, where every body is checked: a
    program checks only the std bodies it uses (sema_check_late)."""
    ok = True
    paths = [p for p in sorted((ROOT / "std").rglob("*.at")) if not p.stem.endswith("_test")]
    for path in paths:
        result = subprocess.run([compiler, "build", str(path), "--stop-after=check",
                                 "--deny-warnings"], capture_output=True, text=True)
        if result.returncode != 0:
            ok = False
            print(f"FAIL {path.relative_to(ROOT)}: does not check as a root module")
            sys.stderr.write(result.stderr)
    print(f"std modules checked whole: {len(paths)}, {'all' if ok else 'not all'} clean")
    return ok


def check_fixtures(compiler: str) -> bool:
    ok = True
    for path in sorted((ROOT / "tests" / "astra_test").glob("*.at")):
        if path.stem.endswith("_test"):
            continue   # loaded with its module
        result = subprocess.run([compiler, "test", str(path)], capture_output=True, text=True)
        expected = path.with_suffix(".out").read_text()
        if result.stdout != expected:
            ok = False
            print(f"FAIL {path.relative_to(ROOT)}: the output of `astra test` differs")
            sys.stdout.write(result.stdout)
    return ok


if __name__ == "__main__":
    sys.exit(main())
