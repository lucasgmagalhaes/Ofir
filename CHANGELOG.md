# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.0] - 2026-11-01

Milestone M2: stage 0 is complete.

### Added

- Generics, `Box`, `List`, slices and references, `Option`/`Result` with
  `?` and `??`, `Map`, exhaustive `match`, warnings (phase 2).
- Ownership: move analysis, drops with drop flags and glue, `.clone()`
  derived for structs, enums and tuples (phase 3).
- `for x in iterator`; `str` splitting, searching and trimming;
  `std.strings`, `std.io`, `std.fs` and `std.os`; `eprint`/`eprintln`.
- Stack overflow detection in the runtime.
- `@test` functions and `astra test`; std tests run in CI.
- Generated-code benchmarks against C (`bench/run_programs.py`).
- Memory checks: `make check-leaks` (valgrind), `make check-asan`, and a
  mutation fuzzer for the compiler.

### Earlier

- Language specification draft (`docs/spec`), standard library reference
  (`docs/std`), architecture documents and decision records.
- Implementation roadmap and risk register.
- Project skeleton, build system and test runner.
- Stage 0 compiler (C): lexer, parser, name resolution, type checker, AIR,
  lowering, LLVM IR generation and the `build`/`run` commands (milestone M1).
- Runtime library: entry point, panics, buffered standard output, strings.
- Benchmarks for every phase (`bench/`) with a regression gate.
