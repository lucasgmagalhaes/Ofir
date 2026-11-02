# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Stage 1 (roadmap phase 4): `compiler/base`, `compiler/diag`, the lexer
  and the parser, whose tokens and trees equal stage 0's on every source,
  `compiler/` included (milestone M3, `make compare-stages`); a project
  root is the nearest directory with an `ofir.toml`.
- Stage 1 module loading and name resolution (roadmap 4.5), checked against
  stage 0's new `--emit=names` trace by `make compare-stages`.
- `fs.canonicalize`.
- Stage 1 type checker (roadmap 4.6), checked against stage 0's new
  `--emit=types` trace by `make compare-stages`.
- Stage 1 move analysis (roadmap 4.7); `--emit=types` now runs the move
  analysis, so its errors are compared too.
- Stage 1 AIR and lowering (roadmap 4.7): `main air` equals stage 0's
  `--emit=air` on every program of the repository, stage 1 itself
  included; drops, clones, hashes and monomorphization included.
- Stage 1 LLVM codegen (roadmap 4.8): `main llvm` equals stage 0's
  `--emit=llvm`; `main build` and `main run` compile programs with clang,
  and the run and panic tests pass with stage 1 (`make test-stage1`).
- Stage 1 driver (roadmap 4.9): stage 0's command line, `ofir test`
  included; every test passes with stage 1 as the compiler.
- `checkedAdd`, `checkedSub`, `checkedMul`, `checkedDiv` and `parse` for
  the integer types, `parse` for the float types, `ParseError`.

### Fixed

- A reference to a binding of a `match` could be returned from a function
  (`return match x { .A(v) => &v }`): it pointed to a copy that ends with
  the function (E0702).
- A type path with an unknown module (`nowhere.T`) was an error type
  without a message (E0200).

- Matching the value in a box bound its parts by move and freed them twice.
- A `let` in a loop kept the parts moved out in the previous iteration.
- `--emit=ast` crashed on a tree with a missing part after a syntax error.
- Stage 1's parser looped forever on a `.` at the end of the file (`x.`).
- Stage 0 left `deny_warnings` uninitialized, which turned warnings into
  errors on macOS.

### Changed

- The language is renamed Ofir (ADR 0025): source files end in `.of`, the
  compiler is `ofir`, the manifest `ofir.toml`, the environment variables
  `OFIR_*` and the runtime symbols `ofir_*`.
- The manifest is `ofir.json`, JSON with comments and trailing commas
  (ADR 0026), instead of `ofir.toml`.
- A directory's module file is `index.of` (was `mod.of`).
- Tests live in a `name_test.of` file next to the module, part of the
  module; only `ofir test` loads it. The std tests moved there.
- Style: no inline blocks (`"fmt": {"inlineBlocks": false}`). Function and
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
- `@test` functions and `ofir test`; std tests run in CI.
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
