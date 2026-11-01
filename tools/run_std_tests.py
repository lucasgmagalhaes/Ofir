#!/usr/bin/env python3
"""Runs the `@test` functions of the standard library (roadmap 3.11).

Every std module that declares a test is compiled with `astra test`, which
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


def modules_with_tests() -> list[Path]:
    found = []
    for path in sorted((ROOT / "std").rglob("*.at")):
        if any(line.startswith("@test") for line in path.read_text().splitlines()):
            found.append(path)
    return found


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
    print(f"std tests: {total} tests in {modules} modules, "
          f"{len(failed)} modules with failures")
    return 1 if failed or not check_fixtures(args.compiler) else 0


def check_fixtures(compiler: str) -> bool:
    ok = True
    for path in sorted((ROOT / "tests" / "astra_test").glob("*.at")):
        result = subprocess.run([compiler, "test", str(path)], capture_output=True, text=True)
        expected = path.with_suffix(".out").read_text()
        if result.stdout != expected:
            ok = False
            print(f"FAIL {path.relative_to(ROOT)}: the output of `astra test` differs")
            sys.stdout.write(result.stdout)
    return ok


if __name__ == "__main__":
    sys.exit(main())
