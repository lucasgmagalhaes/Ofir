# bench — benchmarks

Performance programs (`programs/`) with equivalent C, Rust and Go versions
(`baselines/`). Target: within 10% of C compiled with `clang -O2`.

`python3 bench/run_programs.py --compiler build/release/astra` builds each
program and its C version, checks that their outputs agree and prints the
median wall times and their ratio (results in `HISTORY.md`).
