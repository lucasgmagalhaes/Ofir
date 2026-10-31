# Benchmark history

Measurements of `bench/run_bench.py` at each `perf` commit and milestone.
Instruction counts are comparable across machines; wall times only on the
same machine.

Machine A: Intel Xeon @ 2.10GHz, 4 cores, Linux 6.18, gcc 13.3, release build (`-O2`).

## lex-100k (100 000 lines, 4.0 MB)

Up to `5abdd0c` the input had 871 037 tokens. From `e66fa35` the generator
parenthesizes nested binary expressions (886 577 tokens), so later numbers
are not comparable with earlier ones.

| Commit | Change | Instructions | Wall (ms) | Lines/s | Peak KiB |
|--------|--------|-------------:|----------:|--------:|---------:|
| `80c3480` | baseline: first stage 0 lexer | 652,978,395 | 103.3 | 0.97M | 37,600 |
| `71a2939` | keywords classified by the interner | 264,594,490 | 55.4 | 1.81M | 37,532 |
| `5abdd0c` | files read into the arena, lines indexed with memchr | 210,315,016 | 49.2 | 2.03M | 33,880 |
| `e66fa35` | new input (valid comparisons) | 211,833,128 | 51.2 | 1.95M | 34,440 |

Profile of the current version (callgrind, share of instructions): `lex`
35%, `emit` 13%, `intern` 7% + `sv_hash` 6%, `lex_number` 5%,
`lex_string_part` 4%, `strtod` 4%. Next candidates: reserving the token
list up front and a faster hash for short identifiers.

## parse-100k (100 000 lines, lex + parse)

| Commit | Change | Instructions | Wall (ms) | Lines/s | Peak KiB |
|--------|--------|-------------:|----------:|--------:|---------:|
| `e66fa35` | baseline: first stage 0 parser | 532,273,215 | 141.5 | 0.71M | 91,528 |
| `8ff66f0` | doc comment tokens dropped in place, not copied | 533,476,261 | 122.1 | 0.82M | 64,008 |

The parser itself costs about 320M instructions, 1.5 times the lexer.
Profile (callgrind, parse-10k, share of all instructions): `lex` 19%,
precedence climbing (`level` + `operator_at`) 9%, `arena_alloc` 5%, token
helpers (`at`, `peek`, `accept`) 8%, `malloc`/`free` 4% from the vectors
that collect list items. Next candidates: one table lookup per binary
operator instead of one call per precedence level, inlining the token
helpers, and building lists directly in the arena. Memory: 32-byte tokens
(28 MB at 100k lines) dominate the peak.
