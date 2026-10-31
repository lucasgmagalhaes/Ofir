# Project structure

This document describes the layout of the Astra repository and the
responsibility of each directory. It is the reference for where new code
belongs.

## Overview

```
astra/
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── CHANGELOG.md
├── Makefile                  top-level build: stage0, stage1, stage2, test, verify
├── .editorconfig
├── .github/
│   └── workflows/
│       ├── ci.yml            build all stages, run all test suites
│       └── release.yml       build release binaries
│
├── docs/
│   ├── spec/                 language specification (normative)
│   ├── std/                  standard library reference (normative)
│   ├── errors/               error code registry and explanations
│   ├── design/               architecture and design documents
│   ├── adr/                  architecture decision records
│   ├── bootstrap.md          self-hosting plan and bootstrap chain
│   ├── roadmap.md            implementation roadmap
│   └── risks.md              risk register
│
├── bootstrap/                stage 0 compiler, written in C
├── compiler/                 stage 1 compiler, written in Astra
├── runtime/                  C runtime linked into every Astra program
├── std/                      standard library, written in Astra
├── tests/                    language and compiler test suites
├── examples/                 example programs
├── bench/                    performance benchmarks
├── seed/                     SEEDS.md: checksums of released compiler seeds
└── tools/                    developer scripts and auxiliary tools
```

## `bootstrap/` — stage 0 compiler (C)

```
bootstrap/
├── Makefile
├── README.md
├── src/
│   ├── main.c                command line parsing
│   ├── driver.c/.h           pipeline: runs the phases on a session, invokes clang
│   ├── session.c/.h          state shared by all phases (arena, sources, names, diagnostics)
│   ├── base/                 foundation, no compiler knowledge
│   │   ├── arena.c/.h        arena allocator (all compiler memory)
│   │   ├── vec.h             generic dynamic array macros
│   │   ├── map.c/.h          hash map (string interning, symbol tables)
│   │   ├── str.c/.h          string builder and string views
│   │   ├── intern.c/.h       interned identifiers
│   │   └── source.c/.h       file table, spans, line and column lookup
│   ├── diag/
│   │   └── diag.c/.h         diagnostics: collection and rendering
│   ├── syntax/
│   │   ├── token.c/.h        token kinds, names and printing
│   │   ├── lexer.c/.h        source → tokens: main loop, comments, words, punctuation
│   │   ├── lexer_internal.h  state shared by the lexer files
│   │   ├── literals.c        numbers, strings and byte literals
│   │   ├── ast.h             AST node definitions
│   │   ├── ast_print.c       AST dump for debugging and tests
│   │   └── parser.c/.h       tokens → AST
│   ├── sema/
│   │   ├── types.c/.h        type representation and interning
│   │   ├── scope.c/.h        scopes and symbols
│   │   ├── resolve.c         name resolution, module loading
│   │   ├── check.c           type checking and inference
│   │   └── generics.c        generic signatures and bounds
│   ├── air/
│   │   ├── air.h             Astra Intermediate Representation
│   │   ├── lower.c           typed AST → AIR
│   │   ├── moves.c           moves, definite initialization, drop flags
│   │   ├── mono.c            monomorphization
│   │   └── air_print.c       AIR dump for debugging and tests
│   └── codegen/
│       ├── llvm.c/.h         AIR → textual LLVM IR
│       └── abi.c/.h          type layout and calling convention
└── tests/                    unit tests for the C code (base/, lexer)
```

Stage 0 implements only the bootstrap subset (see `docs/bootstrap.md`). It
mirrors the structure of stage 1 so that porting is mechanical.

## `compiler/` — stage 1 compiler (Astra)

```
compiler/
├── main.at                   command line entry point
├── driver/
│   ├── cli.at                argument parsing, subcommands
│   ├── session.at            compilation options, target, paths
│   ├── pipeline.at           runs the phases in order
│   └── toolchain.at          locating and invoking clang / linker
├── base/
│   ├── arena.at
│   ├── intern.at
│   ├── span.at               source positions, file table
│   └── index_vec.at          typed indices (NodeId, TypeId, ...)
├── diag/
│   ├── diagnostic.at         error/warning/note model
│   ├── render.at             terminal rendering with source excerpts
│   └── codes.at              stable error codes (E0001, ...)
├── syntax/
│   ├── token.at              token kinds and values
│   ├── cursor.at             position in a source file
│   ├── literals.at           numbers, strings and byte literals
│   ├── lexer.at              main loop, comments, words, punctuation
│   ├── ast.at
│   ├── parser.at
│   └── printer.at            AST pretty printer (shared with formatter)
├── sema/
│   ├── types.at
│   ├── scope.at
│   ├── resolve.at
│   ├── check_items.at        declarations, signatures
│   ├── check_exprs.at        expressions and statements
│   ├── infer.at              type inference (unification)
│   ├── interfaces.at         interface satisfaction, vtables
│   ├── generics.at           bounds checking, instantiation
│   ├── patterns.at           pattern typing and exhaustiveness
│   └── consts.at             compile-time evaluation
├── borrow/
│   ├── moves.at              move and initialization analysis
│   ├── borrows.at            borrow checking (non-lexical)
│   └── escape.at             second-class reference rules
├── air/
│   ├── air.at
│   ├── lower.at
│   ├── drops.at              drop elaboration
│   ├── mono.at               monomorphization
│   ├── validate.at           AIR invariants checker
│   └── printer.at
├── opt/                      Astra-level passes before LLVM [planned]
│   └── inline_small.at
└── codegen/
    ├── llvm/
    │   ├── emit.at           AIR → LLVM IR text
    │   ├── types.at          Astra types → LLVM types
    │   ├── abi.at            calling convention, C ABI
    │   ├── intrinsics.at  overflow checks, memcpy, ...
    │   └── debuginfo.at   DWARF metadata [planned]
    └── layout.at             size, alignment, field offsets, niches
```

## `runtime/` — C runtime

```
runtime/
├── include/
│   └── astra_rt.h            interface used by generated code
├── src/
│   ├── entry.c               process entry, argument capture, exit
│   ├── panic.c               panic reporting and abort
│   ├── alloc.c               global allocator wrapper
│   ├── io.c                  unbuffered write/read primitives
│   └── platform/
│       ├── linux.c
│       └── macos.c
└── Makefile
```

The runtime is deliberately small. Everything that can be written in
Astra lives in `std/`.

## `std/` — standard library (Astra)

Module paths follow directories (spec 7.1): `std/collections/map.at` is the
module `std.collections.map`, and a directory's `mod.at` file is the module
named after the directory (`std/fs/mod.at` is `std.fs`).

```
std/
├── prelude.at                names imported into every module (spec 7.4)
├── core/                     no allocation required
│   ├── option.at             Option<T>
│   ├── result.at             Result<T, E>
│   ├── ordering.at           Ordering
│   ├── interfaces.at         Eq, Ord, Hash, Clone, Display, Debug, Iterator, ...
│   ├── num.at                integer and float methods
│   ├── slice.at              methods of [T]
│   └── panic.at
├── mem/                      std.mem: sizeOf, swap, replace, take, UnsafeCell, Cell
├── collections/              std.collections: requires a heap
│   ├── box.at                Box<T>
│   ├── list.at               List<T>
│   └── map.at                Map<K, V>
├── strings/                  std.strings: str methods and helpers
├── fmt/                      std.fmt: formatting used by interpolation
├── io/                       std.io
├── fs/                       std.fs
├── os/                       std.os
├── math/                     std.math
├── testing/                  std.testing: assertEq, Bencher, test harness
├── thread/                   std.thread [planned]
└── sync/                     std.sync   [planned]
```

## `tests/` — test suites

```
tests/
├── README.md                 how to write and run tests
├── runner/
│   └── run_tests.py          test driver (developer dependency only)
├── lex/                      *.at + *.tokens   expected token streams
├── parse/                    *.at + *.ast      expected AST dumps
├── run/                      *.at + *.out      compile, run, compare stdout
├── compile_fail/             *.at with `// error[E0300]: message` annotations
├── panic/                    *.at with `// panic: message` annotations
├── borrow/                   ownership and borrow checker cases
├── codegen/                  *.at + *.ll       FileCheck-style IR checks
└── selfhost/                 stage2 == stage3 verification
```

Every bug fix adds a test. Each test file is small and focused on one
behavior.

## `bench/`, `examples/`, `tools/`

```
bench/
├── programs/                 fib, nbody, binary_trees, spectral_norm, ...
├── baselines/                equivalent C, Rust and Go versions
└── run_bench.py

examples/                     showcase programs, all compiled by CI

tools/
├── bootstrap.sh              build stage0 → stage1 → stage2 from scratch
├── verify_selfhost.sh        compare stage2 and stage3 outputs
├── fmt/                      `astra fmt` [planned]
└── lsp/                      language server [planned]
```

## Dependency rules

```
examples, tests, bench ──► compiler / bootstrap ──► runtime, std
                                │
             driver ─► syntax ─► sema ─► air ─► borrow ─► codegen
                │         ▲        ▲       ▲       ▲         ▲
                └─────────┴─── base, diag (used by all phases) ┘
```

- A phase may depend only on earlier phases and on `base` and `diag`.
- `syntax` knows nothing about types; `sema` never produces LLVM IR.
- `borrow` analyses AIR (a control-flow graph), not the AST.
- `codegen` reads only AIR, never the AST.
- `runtime` must not depend on `std`; `std/core` must not allocate.
- Types and methods are documented in the [standard library
  reference](../std/README.md).
