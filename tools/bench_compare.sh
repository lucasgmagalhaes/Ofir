#!/bin/sh
# Compares the performance of the working tree with a base revision.
#
#     tools/bench_compare.sh [base-revision]      (default: origin/main)
#
# Builds both compilers in release mode and runs bench/run_bench.py with
# --compare, which fails on an instruction count regression.
set -eu

BASE=${1:-origin/main}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK="$ROOT/build/bench-base"

rm -rf "$WORK"
git -C "$ROOT" worktree add --detach --force "$WORK/src" "$BASE" >/dev/null
trap 'git -C "$ROOT" worktree remove --force "$WORK/src"' EXIT

# Each compiler is built two levels below its own tree, as `build/release`,
# so that it finds that tree's standard library at `../../std`.
make -C "$WORK/src/bootstrap" release BUILD="$WORK/src/build/release" >/dev/null
make -C "$ROOT/bootstrap" release BUILD="$ROOT/build/release" >/dev/null
python3 "$ROOT/bench/run_bench.py" --compiler "$ROOT/build/release/astra" \
    --compare "$WORK/src/build/release/astra" --json "$ROOT/build/bench-results.json"
