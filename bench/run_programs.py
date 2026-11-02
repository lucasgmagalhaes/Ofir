#!/usr/bin/env python3
"""Generated-code benchmarks (roadmap 3.11a): each program of `programs/`
against its C version in `baselines/`, both optimized (`ofir build
--release`, `clang -O2`). Prints the median wall time of each and the
ratio Ofir / C; the outputs must agree.

    python3 bench/run_programs.py --compiler build/release/ofir [--runs 5]
"""

import argparse
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def build(compiler: str, program: Path, work: Path) -> tuple[Path, Path]:
    ofir = work / (program.stem + "-ofir")
    c = work / (program.stem + "-c")
    subprocess.run([compiler, "build", str(program), "--release", "-o", str(ofir)], check=True)
    subprocess.run(["clang", "-O2", str(ROOT / "baselines" / (program.stem + ".c")), "-lm",
                    "-o", str(c)], check=True)
    return ofir, c


def measure(exe: Path, runs: int) -> tuple[float, str]:
    times = []
    output = ""
    for _ in range(runs):
        start = time.perf_counter()
        output = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
        times.append(time.perf_counter() - start)
    return statistics.median(times), output


def same_output(a: str, b: str) -> bool:
    """Equal, or equal numbers: floats are printed with different digits."""
    wa, wb = a.split(), b.split()
    if len(wa) != len(wb):
        return False
    for x, y in zip(wa, wb):
        if x != y:
            try:
                if abs(float(x) - float(y)) > 1e-9 * max(1.0, abs(float(y))):
                    return False
            except ValueError:
                return False
    return True


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--runs", type=int, default=5)
    args = parser.parse_args()
    ok = True
    print(f"{'program':<14} {'ofir ms':>9} {'c ms':>9} {'ratio':>6}")
    with tempfile.TemporaryDirectory() as tmp:
        for program in sorted((ROOT / "programs").glob("*.of")):
            ofir, c = build(args.compiler, program, Path(tmp))
            ofir_time, ofir_out = measure(ofir, args.runs)
            c_time, c_out = measure(c, args.runs)
            agree = same_output(ofir_out, c_out)
            ok = ok and agree
            note = "" if agree else "  OUTPUTS DIFFER"
            print(f"{program.stem:<14} {ofir_time * 1000:9.1f} {c_time * 1000:9.1f} "
                  f"{ofir_time / c_time:6.2f}{note}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
