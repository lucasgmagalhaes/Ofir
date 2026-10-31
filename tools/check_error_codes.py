#!/usr/bin/env python3
"""Checks the error code registry (docs/errors/README.md).

- every code in the registry table appears once;
- every code annotated in tests/ (`// error[E0300]: ...`) is registered.

The runner self-test fixtures (tests/runner/) are ignored.
"""

import re
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "docs" / "errors" / "README.md"
ROW_RE = re.compile(r"^\|\s*([EW]\d{4})\s*\|", re.MULTILINE)
USE_RE = re.compile(r"(?:error|warning)\[([EW]\d{4})\]")


def unknown_codes(known: set) -> list[str]:
    problems = []
    for path in sorted((ROOT / "tests").rglob("*.at")):
        if "runner" in path.relative_to(ROOT / "tests").parts:
            continue
        for number, line in enumerate(path.read_text().splitlines(), start=1):
            problems += [f"{path.relative_to(ROOT)}:{number}: {code} is not in "
                         f"{REGISTRY.relative_to(ROOT)}"
                         for code in USE_RE.findall(line) if code not in known]
    return problems


def main() -> int:
    registered = ROW_RE.findall(REGISTRY.read_text())
    problems = [f"{code} is registered {n} times"
                for code, n in Counter(registered).items() if n > 1]
    known = set(registered)
    problems += unknown_codes(known)
    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if problems:
        return 1
    print(f"error codes: {len(known)} registered, all test annotations known")
    return 0


if __name__ == "__main__":
    sys.exit(main())
