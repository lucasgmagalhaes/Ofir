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
│   ├── design/               architecture and design documents
│   ├── adr/                  architecture decision records
│   ├── bootstrap.md          self-hosting plan
│   └── roadmap.md            implementation roadmap
│
├── bootstrap/                stage 0 compiler, written in C
├── compiler/                 stage 1 compiler, written in Astra
├── runtime/                  C runtime linked into every Astra program
├── std/                      standard library, written in Astra
├── tests/                    language and compiler test suites
├── examples/                 example programs
├── bench/                    performance benchmarks
└── tools/                    developer scripts and auxiliary tools
```

## `bootstrap/` — stage 0 compiler (C)

```
bootstrap/
├── Makefile
├── README.md
├── src/
│   ├── main.c                command line entry point
│   ├── driver.c/.h           pipeline orchestration, invoking clang
│   ├── base/                 foundation, no compiler knowledge
│   │   ├── arena.c/.h        arena allocator (all compiler memory)
│   │   ├── vec.h             generic dynamic array macros
│   │   ├── map.c/.h          hash map (string interning, symbol tables)
│   │   ├── str.c/.h          string builder and string views
│   │   ├── intern.c/.h       interned identifiers
│   │   └── fs.c/.h           file reading, paths
│   ├── diag/
│   │   └── diag.c/.h         diagnostics: collection and rendering
│   ├── syntax/
│   │   ├── token.h           token kinds
│   │   ├── lexer.c/.h        source → tokens
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
├── main.astra                command line entry point
├── driver/
│   ├── cli.astra             argument parsing, subcommands
│   ├── session.astra         compilation options, target, paths
│   ├── pipeline.astra        runs the phases in order
│   └── toolchain.astra       locating and invoking clang / linker
├── base/
│   ├── arena.astra
│   ├── intern.astra
│   ├── span.astra            source positions, file table
│   └── index_vec.astra       typed indices (NodeId, TypeId, ...)
├── diag/
│   ├── diagnostic.astra      error/warning/note model
│   ├── render.astra          terminal rendering with source excerpts
│   └── codes.astra           stable error codes (E0001, ...)
├── syntax/
│   ├── token.astra
│   ├── lexer.astra
│   ├── ast.astra
│   ├── parser.astra
│   └── printer.astra         AST pretty printer (shared with formatter)
├── sema/
│   ├── types.astra
│   ├── scope.astra
│   ├── resolve.astra
│   ├── check_items.astra     declarations, signatures
│   ├── check_exprs.astra     expressions and statements
│   ├── infer.astra           type inference (unification)
│   ├── interfaces.astra      interface satisfaction, vtables
│   ├── generics.astra        bounds checking, instantiation
│   ├── patterns.astra        pattern typing and exhaustiveness
│   └── consts.astra          compile-time evaluation
├── borrow/
│   ├── moves.astra           move and initialization analysis
│   ├── borrows.astra         borrow checking (non-lexical)
│   └── escape.astra          second-class reference rules
├── air/
│   ├── air.astra
│   ├── lower.astra
│   ├── drops.astra           drop elaboration
│   ├── mono.astra            monomorphization
│   ├── validate.astra        AIR invariants checker
│   └── printer.astra
├── opt/                      Astra-level passes before LLVM [planned]
│   └── inline_small.astra
└── codegen/
    ├── llvm/
    │   ├── emit.astra        AIR → LLVM IR text
    │   ├── types.astra       Astra types → LLVM types
    │   ├── abi.astra         calling convention, C ABI
    │   ├── intrinsics.astra  overflow checks, memcpy, ...
    │   └── debuginfo.astra   DWARF metadata [planned]
    └── layout.astra          size, alignment, field offsets, niches
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

```
std/
├── core/                     always available; no allocation required
│   ├── prelude.astra         names imported into every module
│   ├── option.astra
│   ├── result.astra
│   ├── interfaces.astra      Eq, Ord, Hash, Clone, Display, Debug, Iterator, ...
│   ├── num.astra             integer and float methods
│   ├── slice.astra
│   ├── mem.astra             size_of, swap, replace, raw allocation
│   └── panic.astra
├── alloc/                    requires a heap
│   ├── box.astra
│   ├── list.astra
│   ├── map.astra
│   ├── str.astra
│   └── fmt.astra             formatting used by interpolation
├── io/
├── fs/
├── os/
├── math/
├── testing/                  assert_eq, Bencher, test harness
├── thread/                   [planned]
└── sync/                     [planned]
```

## `tests/` — test suites

```
tests/
├── README.md                 how to write and run tests
├── runner/
│   └── run_tests.py          test driver (developer dependency only)
├── lex/                      *.astra + *.tokens   expected token streams
├── parse/                    *.astra + *.ast      expected AST dumps
├── run/                      *.astra + *.out      compile, run, compare stdout
├── compile_fail/             *.astra with `// error[E0123]: message` annotations
├── panic/                    *.astra with `// panic: message` annotations
├── borrow/                   ownership and borrow checker cases
├── codegen/                  *.astra + *.ll       FileCheck-style IR checks
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
