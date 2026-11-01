#!/usr/bin/env python3
"""Compiler benchmarks.

Measures each benchmark with a release build of the compiler:

- wall time: median of several runs (noisy, for humans);
- peak memory (maximum resident set size);
- instructions executed, counted by valgrind (deterministic, for CI).

With --compare, runs the same benchmarks with a second compiler (usually a
build of the base branch) and fails if the instruction count regresses by
more than --max-regression percent. See docs/design/performance.md.
"""

import argparse
import json
import os
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INPUTS = ROOT / "build" / "bench"

# name -> (generated input lines, compiler arguments after the input path)
# name: (lines, generator profile, compiler arguments)
BENCHMARKS = {
    "lex-10k": (10_000, "syntax", ["--stop-after=lex"]),
    "lex-100k": (100_000, "syntax", ["--stop-after=lex"]),
    "parse-10k": (10_000, "syntax", ["--stop-after=parse"]),
    "parse-100k": (100_000, "syntax", ["--stop-after=parse"]),
    "check-10k": (10_000, "typed", ["--stop-after=check"]),
    "check-100k": (100_000, "typed", ["--stop-after=check"]),
    "llvm-10k": (10_000, "typed", ["--emit=llvm", "-o", str(INPUTS / "out-10k")]),
    "llvm-100k": (100_000, "typed", ["--emit=llvm", "-o", str(INPUTS / "out-100k")]),
}


def input_file(lines: int, profile: str) -> Path:
    path = INPUTS / f"input-{profile}-{lines}.at"
    if not path.exists():
        INPUTS.mkdir(parents=True, exist_ok=True)
        source = subprocess.run([sys.executable, str(ROOT / "bench" / "gen_source.py"),
                                 "--lines", str(lines), "--profile", profile],
                                capture_output=True, text=True, check=True)
        path.write_text(source.stdout)
    return path


def run_once(cmd: list[str]) -> tuple[float, int]:
    """Returns the wall time in milliseconds and the peak RSS in KiB."""
    # stderr goes to a file: a pipe nobody reads would block a compiler that
    # reports many diagnostics.
    with tempfile.TemporaryFile() as errors:
        start = time.perf_counter()
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=errors)
        _, status, usage = os.wait4(proc.pid, 0)
        elapsed = (time.perf_counter() - start) * 1000
        if os.waitstatus_to_exitcode(status) != 0:
            errors.seek(0)
            sys.exit(f"benchmark command failed: {' '.join(cmd)}\n{errors.read().decode()}")
    rss = usage.ru_maxrss if sys.platform != "darwin" else usage.ru_maxrss // 1024
    return elapsed, rss


def instructions(cmd: list[str]) -> int | None:
    if not shutil.which("valgrind"):
        return None
    proc = subprocess.run(["valgrind", "--tool=cachegrind", "--cache-sim=no",
                           "--cachegrind-out-file=/dev/null", *cmd],
                          stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    m = re.search(r"I\s+refs:\s+([\d,]+)", proc.stderr)
    return int(m.group(1).replace(",", "")) if m else None


def measure(compiler: str, runs: int, count_instructions: bool) -> dict:
    results = {}
    for name, (lines, profile, args) in BENCHMARKS.items():
        cmd = [compiler, "build", str(input_file(lines, profile)), *args]
        samples = [run_once(cmd) for _ in range(runs)]
        wall = statistics.median(s[0] for s in samples)
        results[name] = {
            "lines": lines,
            "wall_ms": round(wall, 2),
            "lines_per_s": round(lines / (wall / 1000)),
            "peak_rss_kib": max(s[1] for s in samples),
            "instructions": instructions(cmd) if count_instructions else None,
        }
    return results


def change(new: float, old: float) -> str:
    return f"{(new - old) / old * 100:+.1f}%" if old else "n/a"


def print_table(results: dict, base: dict | None) -> None:
    header = f"{'benchmark':<10} {'wall ms':>9} {'lines/s':>11} {'peak KiB':>9} {'instructions':>15}"
    print(header + ("   vs base (wall / instr)" if base else ""))
    for name, r in results.items():
        instr = f"{r['instructions']:,}" if r["instructions"] else "-"
        line = (f"{name:<10} {r['wall_ms']:>9.2f} {r['lines_per_s']:>11,} "
                f"{r['peak_rss_kib']:>9,} {instr:>15}")
        if base and name in base:
            b = base[name]
            instr_change = change(r["instructions"], b["instructions"]) if r["instructions"] else ""
            line += f"   {change(r['wall_ms'], b['wall_ms'])} / {instr_change}"
        print(line)


def regressions(results: dict, base: dict, limit: float) -> list[str]:
    found = []
    for name, r in results.items():
        old, new = base.get(name, {}).get("instructions"), r["instructions"]
        if old and new and (new - old) / old * 100 > limit:
            found.append(f"{name}: instructions {change(new, old)} (limit +{limit}%)")
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--compiler", default=str(ROOT / "build" / "release" / "astra"))
    parser.add_argument("--compare", help="a second compiler to compare against (the base)")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--no-instructions", action="store_true", help="skip valgrind")
    parser.add_argument("--max-regression", type=float, default=3.0,
                        help="allowed instruction count increase in percent (default 3)")
    parser.add_argument("--json", type=Path, help="write the results to this file")
    args = parser.parse_args()

    count = not args.no_instructions
    results = measure(args.compiler, args.runs, count)
    base = measure(args.compare, args.runs, count) if args.compare else None
    print_table(results, base)
    if args.json:
        args.json.write_text(json.dumps({"results": results, "base": base}, indent=2) + "\n")
    problems = regressions(results, base, args.max_regression) if base else []
    for problem in problems:
        print(f"error: performance regression: {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
