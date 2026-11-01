# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Stage 1 (roadmap phase 4): `compiler/base`, `compiler/diag`, the lexer
  and the parser, whose tokens and trees equal stage 0's on every source,
  `compiler/` included (milestone M3, `make compare-stages`); a project
  root is the nearest directory with an `astra.toml`.
- Stage 1 module loading and name resolution (roadmap 4.5), checked against
  stage 0's new `--emit=names` trace by `make compare-stages`.
- `fs.canonicalize`.
- `checkedAdd`, `checkedSub`, `checkedMul`, `checkedDiv` and `parse` for
  the integer types, `parse` for the float types, `ParseError`.

### Fixed

- Matching the value in a box bound its parts by move and freed them twice.
- A `let` in a loop kept the parts moved out in the previous iteration.
- `--emit=ast` crashed on a tree with a missing part after a syntax error.

### Changed

- A directory's module file is `index.at` (was `mod.at`).
- Tests live in a `name_test.at` file next to the module, part of the
  module; only `astra test` loads it. The std tests moved there.
- Style: no inline blocks (`[fmt] inline-blocks = false`). Function and
  statement bodies always go on their own lines; `make check-format`
  (`tools/check_format.py`, with `--fix`) enforces it in `make test`.

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
