#!/usr/bin/env python3
"""Compare generated Ofir programs with equivalent C, Rust and Go baselines.

Builds optimized executables, checks their outputs, and reports median wall
times. Use --baselines-only to validate the reference programs without Ofir.
"""

import argparse
import json
import math
import os
import platform
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
LANGUAGES = ("ofir", "c", "rust", "go")


def same_output(a: str, b: str) -> bool:
    """Allow only tiny float formatting differences between languages."""
    wa, wb = a.split(), b.split()
    if len(wa) != len(wb):
        return False
    for x, y in zip(wa, wb):
        if x == y:
            continue
        try:
            left, right = float(x), float(y)
        except ValueError:
            return False
        if not math.isfinite(left) or not math.isfinite(right):
            return False
        if not math.isclose(left, right, rel_tol=1e-9, abs_tol=1e-9):
            return False
    return True


def build(program: Path, work: Path, tools: dict[str, str], languages: tuple[str, ...]) -> dict[str, Path]:
    executables = {}
    for language in languages:
        exe = work / f"{program.stem}-{language}{'.exe' if os.name == 'nt' else ''}"
        suffix = "rs" if language == "rust" else language
        source = ROOT / "baselines" / f"{program.stem}.{suffix}"
        if language == "ofir":
            cmd = [tools["ofir"], "build", str(program), "--release", "-o", str(exe)]
        elif language == "c":
            cmd = [tools["c"], "-O2", str(source), "-lm", "-o", str(exe)]
        elif language == "rust":
            cmd = [tools["rust"], "-C", "opt-level=2", str(source), "-o", str(exe)]
        else:
            cmd = [tools["go"], "build", "-o", str(exe), str(source)]
        built = subprocess.run(cmd, capture_output=True, text=True)
        if built.returncode != 0:
            raise ValueError(f"{program.stem}: {language} build failed\n{built.stderr}")
        executables[language] = exe
    return executables


def run_once(exe: Path) -> tuple[float, str]:
    start = time.perf_counter()
    result = subprocess.run([str(exe)], check=True, capture_output=True, text=True)
    return time.perf_counter() - start, result.stdout


def tool_version(language: str, tool: str) -> str:
    flag = "version" if language in ("ofir", "go") else "--version"
    result = subprocess.run([tool, flag], check=True, capture_output=True, text=True)
    return (result.stdout or result.stderr).splitlines()[0]


def benchmark(program: Path, work: Path, tools: dict[str, str], languages: tuple[str, ...],
              runs: int, warmups: int) -> dict[str, dict[str, float]]:
    executables = build(program, work, tools, languages)
    reference = run_once(executables["c"])[1]
    for language in languages:
        output = run_once(executables[language])[1]
        if not same_output(output, reference):
            raise ValueError(f"{program.stem}: {language} output differs from C\n"
                             f"C: {reference}\n{language}: {output}")
    for _ in range(warmups):
        for language in languages:
            run_once(executables[language])
    samples = {language: [] for language in languages}
    for round_number in range(runs):
        order = languages[round_number % len(languages):] + languages[:round_number % len(languages)]
        for language in order:
            elapsed, output = run_once(executables[language])
            if not same_output(output, reference):
                raise ValueError(f"{program.stem}: {language} output changed during measurement")
            samples[language].append(elapsed)
    c_time = statistics.median(samples["c"])
    return {language: {"wall_ms": round(statistics.median(samples[language]) * 1000, 3),
                       "vs_c": round(statistics.median(samples[language]) / c_time, 3)}
            for language in languages}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--compiler", default=str(ROOT.parent / "build" / "release" / "ofir"))
    parser.add_argument("--clang", default=os.environ.get("OFIR_CLANG", "clang"))
    parser.add_argument("--rustc", default="rustc")
    parser.add_argument("--go", default="go")
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--warmups", type=int, default=1)
    parser.add_argument("--baselines-only", action="store_true")
    parser.add_argument("--json", type=Path, help="write results and toolchain versions to JSON")
    args = parser.parse_args()
    if args.runs < 1 or args.warmups < 0:
        parser.error("--runs must be positive and --warmups cannot be negative")
    tools = {"ofir": args.compiler, "c": args.clang, "rust": args.rustc, "go": args.go}
    languages = LANGUAGES[1:] if args.baselines_only else LANGUAGES
    for language in languages:
        if not shutil.which(tools[language]):
            parser.error(f"{language} compiler not found: {tools[language]}")
    versions = {language: tool_version(language, tools[language]) for language in languages}
    results = {}
    try:
        with tempfile.TemporaryDirectory() as tmp:
            for program in sorted((ROOT / "programs").glob("*.of")):
                results[program.stem] = benchmark(program, Path(tmp), tools, languages,
                                                  args.runs, args.warmups)
    except (OSError, subprocess.CalledProcessError, ValueError) as exc:
        print(f"benchmark failed: {exc}", file=sys.stderr)
        return 1
    print(f"{'program':<14} {'language':<8} {'median ms':>10} {'vs C':>7}")
    for program, measurements in results.items():
        for language, values in measurements.items():
            print(f"{program:<14} {language:<8} {values['wall_ms']:10.3f} {values['vs_c']:7.3f}")
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps({"platform": platform.platform(), "runs": args.runs,
                                        "warmups": args.warmups, "toolchains": versions,
                                        "results": results}, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
