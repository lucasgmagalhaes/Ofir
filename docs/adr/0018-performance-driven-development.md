# 0018. Performance-driven development

- Status: Accepted
- Date: 2026-10-31

## Context

"Fast" is a primary goal of Astra, for the compiler and for generated code.
Without measurements, performance work is guesswork and regressions go
unnoticed until they are expensive to fix.

## Decision

- Benchmarks exist from the first phase of the compiler and grow with it
  (`bench/`), with deterministic generated inputs.
- **Instruction counts** (valgrind) gate pull requests: more than 3% on any
  benchmark fails CI unless justified. Wall time and peak memory are
  reported alongside.
- Optimizations follow profiles (callgrind), one per `perf` commit whose
  message states the measured effect; results are logged in
  `bench/HISTORY.md`.

## Consequences

- Every claim about performance is backed by a number in the history.
- CI takes longer (two release builds and valgrind runs per pull request).
- Instruction counts do not capture every effect (cache misses, parallelism);
  wall-time benchmarks on a fixed machine complement them before releases.

First results: the stage 0 lexer went from 0.97M to 2.03M lines/s (−68%
instructions) in two profile-guided commits.
