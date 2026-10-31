# Benchmark history

Measurements of `bench/run_bench.py` at each `perf` commit and milestone.
Instruction counts are comparable across machines; wall times only on the
same machine.

Machine A: Intel Xeon @ 2.10GHz, 4 cores, Linux 6.18, gcc 13.3, release build (`-O2`).

## lex-100k (100 000 lines, 4.0 MB, 871 037 tokens)

| Commit | Change | Instructions | Wall (ms) | Lines/s | Peak KiB |
|--------|--------|-------------:|----------:|--------:|---------:|
| `80c3480` | baseline: first stage 0 lexer | 652,978,395 | 103.3 | 0.97M | 37,600 |
| `71a2939` | keywords classified by the interner | 264,594,490 | 55.4 | 1.81M | 37,532 |
| `5abdd0c` | files read into the arena, lines indexed with memchr | 210,315,016 | 49.2 | 2.03M | 33,880 |

Profile of the current version (callgrind, share of instructions): `lex`
35%, `emit` 13%, `intern` 7% + `sv_hash` 6%, `lex_number` 5%,
`lex_string_part` 4%, `strtod` 4%. Next candidates: reserving the token
list up front and a faster hash for short identifiers.
