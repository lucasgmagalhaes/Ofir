# Changelog

All notable changes to this project are documented in this file. The format
follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and the
project uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added

- Language specification draft (`docs/spec`), standard library reference
  (`docs/std`), architecture documents and decision records.
- Implementation roadmap and risk register.
- Project skeleton, build system and test runner.
- Stage 0 compiler (C): lexer, parser, name resolution, type checker, AIR,
  lowering, LLVM IR generation and the `build`/`run` commands (milestone M1).
- Runtime library: entry point, panics, buffered standard output, strings.
- Benchmarks for every phase (`bench/`) with a regression gate.
