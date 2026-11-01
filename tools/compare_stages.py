#!/usr/bin/env python3
"""Compares the stage 1 compiler (compiler/, built by stage 0) with stage 0
(roadmap phase 4): for every Astra file of the repository, each command
that stage 1 implements so far must give the same standard output, standard
error and exit status as stage 0's matching option (after an error, the
standard error and the status only: the trees built while recovering need
not agree). `--fuzz N` also
compares N mutated sources (tools/fuzz_frontend.py's mutations).

    python3 tools/compare_stages.py [--compiler build/stage0/astra] [--fuzz 200]
"""

import argparse
import concurrent.futures
import random
import subprocess
import sys
import os
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fuzz_frontend import mutate  # noqa: E402

# stage 1 command -> the stage 0 option that it must match
COMMANDS = {"tokens": "--emit=tokens", "ast": "--emit=ast", "names": "--emit=names"}
SOURCES = ["tests", "std", "examples", "compiler", "bench"]


# Both stages read the same standard library, so that paths in messages agree.
ENV = dict(os.environ, ASTRA_STD=str(ROOT / "std"))


def run(argv: list) -> tuple:
    result = subprocess.run(argv, capture_output=True, timeout=60, env=ENV)
    return result.returncode, result.stdout, result.stderr


def compare(stage0: str, stage1: str, path: Path) -> list:
    problems = []
    for command, option in COMMANDS.items():
        expected = run([stage0, "build", str(path), option, "-o", "/dev/null"])
        actual = run([stage1, command, str(path)])
        if expected[0] != 0 and actual[0] == expected[0] and actual[2] == expected[2]:
            continue   # after errors, the trees of the recovery need not agree
        if expected != actual:
            names = ("status", "stdout", "stderr")
            what = [n for n, a, b in zip(names, expected, actual) if a != b]
            problems.append(f"{path}: `{command}` differs from stage 0 ({', '.join(what)})")
    return problems


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--compiler", default=str(ROOT / "build" / "stage0" / "astra"))
    parser.add_argument("--fuzz", type=int, default=0)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        stage1 = str(Path(tmp) / "astra1")
        built = run([args.compiler, "build", str(ROOT / "compiler" / "main.at"), "-o", stage1])
        if built[0] != 0:
            sys.stderr.write(built[2].decode())
            print("stages: stage 1 does not build")
            return 1
        files = sorted(p for d in SOURCES for p in (ROOT / d).rglob("*.at"))
        rng = random.Random(args.seed)
        for i in range(args.fuzz):
            case = Path(tmp) / f"fuzz{i}.at"
            case.write_text(mutate(rng, rng.choice(files).read_text()))
            files.append(case)
        with concurrent.futures.ThreadPoolExecutor() as pool:
            results = list(pool.map(lambda p: compare(args.compiler, stage1, p), files))
    problems = [p for r in results for p in r]
    for problem in problems:
        print(f"error: {problem}")
    print(f"stages: {len(files)} files ({args.fuzz} mutated), commands {', '.join(COMMANDS)}: "
          f"{len(problems)} differences")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
