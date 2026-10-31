#!/usr/bin/env python3
"""Checks the size rules of docs/style-guide.md ("Size and structure").

For C, Astra and Python sources it measures file length, function length,
nesting depth and parameter count. Exceeding a target is a warning;
exceeding a limit is an error.

A flat dispatch table (a function that is one `match`/`switch` mapping
cases to results) may exceed the function length limit when the line
before the function contains `check-sizes: table`.
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ["bootstrap/src", "compiler", "std", "runtime/src", "tests/runner", "tools"]
SUFFIXES = {".c", ".h", ".at", ".py"}

#               target, limit
FILE_LINES = (400, 600)
FUNCTION_LINES = (40, 60)
NESTING = (3, 4)
PARAMETERS = (5, 7)
LINE_WIDTH = 100    # a hard limit: every longer line is an error


@dataclass
class Function:
    name: str
    line: int          # 1-based line of the signature
    length: int        # lines, signature to closing brace inclusive
    nesting: int       # deepest block level inside the body (body = 0)
    params: int
    table: bool


def strip_literals(line: str) -> str:
    line = re.sub(r"b?'(\\.|[^'\\])'", "''", line)
    line = re.sub(r'"(\\.|[^"\\])*"', '""', line)
    return line.split("//")[0]


def count_params(signature: str) -> int:
    inner = signature[signature.find("(") + 1:]
    depth, params, current = 0, 0, ""
    for ch in inner:
        if ch in "([<{":
            depth += 1
        elif ch in ")]>}":
            if depth == 0:
                break
            depth -= 1
        if ch == "," and depth == 0:
            params += bool(current.strip())
            current = ""
        else:
            current += ch
    params += bool(current.strip())
    return params


def scan_body(lines: list[str], i: int) -> tuple[str, int, int]:
    """From a signature line, returns the signature text, the index after the
    closing brace and the deepest block level (the function body is 0)."""
    signature, depth, deepest, opened = "", 0, 0, False
    while i < len(lines):
        code = strip_literals(lines[i])
        if not opened:
            signature += code
        for ch in code:
            depth += (ch == "{") - (ch == "}")
            deepest = max(deepest, depth - 1)
        opened = opened or "{" in code
        i += 1
        if opened and depth <= 0:
            break
    return signature, i, deepest


def braced_functions(lines: list[str], is_start) -> list[Function]:
    """Functions delimited by braces (C and Astra)."""
    functions = []
    i = 0
    while i < len(lines):
        name = is_start(lines[i])
        if not name:
            i += 1
            continue
        start = i
        signature, i, deepest = scan_body(lines, i)
        table = start > 0 and "check-sizes: table" in lines[start - 1]
        params = count_params(signature)
        if re.search(r"\(\s*(mut |move )?self\b", signature):
            params -= 1
        if re.search(r"\(\s*void\s*\)", signature):
            params = 0
        functions.append(Function(name, start + 1, i - start, deepest, params, table))
    return functions


C_START = re.compile(r"^(?!(if|for|while|switch|return|typedef|struct|enum)\b)"
                     r"[A-Za-z_][\w\s\*]*?\b(\w+)\([^;]*$")


def c_start(line: str):
    m = C_START.match(line)
    return m.group(2) if m and "=" not in line.split("(")[0] else None


def astra_start(line: str):
    m = re.match(r"^\s*(?:pub\s+)?(?:unsafe\s+)?fn\s+(\w+)", line)
    if m and ("{" in strip_literals(line) or line.rstrip().endswith(")")):
        return m.group(1)
    return None


def indentation(line: str) -> int:
    return len(line) - len(line.lstrip())


def python_functions(lines: list[str]) -> list[Function]:
    functions = []
    for i, line in enumerate(lines):
        m = re.match(r"^(\s*)def (\w+)\((.*)", line)
        if not m:
            continue
        indent = len(m.group(1))
        j, deepest = i + 1, 0
        while j < len(lines) and (not lines[j].strip() or indentation(lines[j]) > indent):
            text = lines[j].split("#")[0].rstrip()
            if re.match(r"\s*(if|elif|else|for|while|with|try|except|finally)\b.*:$", text):
                level = (len(lines[j]) - len(lines[j].lstrip()) - indent) // 4
                deepest = max(deepest, level)
            j += 1
        while j > i + 1 and not lines[j - 1].strip():
            j -= 1
        params = count_params(line[line.find("("):])
        if re.match(r"\(\s*self\b", line[line.find("("):]):
            params -= 1
        functions.append(Function(m.group(2), i + 1, j - i, deepest, params, False))
    return functions


def counted_lines(path: Path, lines: list[str]) -> int:
    """File length, not counting the `@test` section at the end of Astra files."""
    if path.suffix == ".at":
        for i, line in enumerate(lines):
            if line.startswith("@test"):
                return i
    return len(lines)


def check(path: Path, report) -> None:
    lines = path.read_text().splitlines()
    rel = path.relative_to(ROOT)
    report((rel, 1), "file", counted_lines(path, lines), FILE_LINES, "lines")
    for number, line in enumerate(lines, start=1):
        report((rel, number), "line", len(line), (LINE_WIDTH, LINE_WIDTH), "characters")
    if path.suffix == ".py":
        functions = python_functions(lines)
    elif path.suffix == ".at":
        functions = braced_functions(lines, astra_start)
    elif path.suffix == ".h":
        # Headers declare functions; only inline definitions are measured.
        functions = braced_functions(lines, lambda l: c_start(l) if "inline" in l else None)
    else:
        functions = braced_functions(lines, c_start)
    for f in functions:
        what = f"function `{f.name}`"
        if not f.table:
            report((rel, f.line), what, f.length, FUNCTION_LINES, "lines")
        report((rel, f.line), what, f.nesting, NESTING, "levels of nesting")
        report((rel, f.line), what, f.params, PARAMETERS, "parameters")


def main() -> int:
    counts = {"warning": 0, "error": 0}

    def report(where, what, value, rule, unit):
        target, limit = rule
        rel, line = where
        if value > limit:
            severity, bound = "error", f"limit {limit}"
        elif value > target:
            severity, bound = "warning", f"target {target}"
        else:
            return
        counts[severity] += 1
        print(f"{severity}: {rel}:{line}: {what} has {value} {unit} ({bound})")

    for source in SOURCES:
        for path in sorted((ROOT / source).rglob("*")):
            if path.suffix in SUFFIXES and "selftest" not in path.parts:
                check(path, report)
    print(f"sizes: {counts['error']} errors, {counts['warning']} warnings")
    return 1 if counts["error"] else 0


if __name__ == "__main__":
    sys.exit(main())
