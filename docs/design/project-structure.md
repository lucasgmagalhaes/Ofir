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
│   │   ├── ast_print*.c      AST dump for `--emit=ast` and tests
│   │   ├── ast_text.c/.h     one-line text for types, patterns and literals
│   │   ├── parser.c/.h       tokens → AST: entry point, token helpers, recovery
│   │   ├── parser_internal.h state shared by the parser files
│   │   ├── parse_items.c     items, imports, extern blocks, attributes
│   │   ├── parse_decls.c     functions, structs and enums
│   │   ├── parse_stmts.c     statements
│   │   ├── parse_control.c   blocks, `if`, `match`, `unsafe`
│   │   ├── parse_exprs.c     operator precedence
│   │   ├── parse_primary.c   primary and postfix expressions
│   │   ├── parse_types.c     types
│   │   └── parse_patterns.c  patterns
│   ├── sema/
│   │   ├── ids.h             TypeId, DefId, LocalId
│   │   ├── types.c/.h        type representation and interning
│   │   ├── sema.c/.h         definitions, modules, `@lang` lookups, phase entry points
│   │   ├── scope.c/.h        one table per module and the local binding stack
│   │   ├── resolve.c         top-level declarations of every module, `@lang`
│   │   ├── resolve_import.c  imports, the prelude, qualified names
│   │   ├── resolve_body.c    names in function bodies
│   │   ├── resolve_internal.h state shared by the resolver files
│   │   ├── check.c           type checking: modules, items, extern blocks
│   │   ├── check_types.c     type expressions and signatures
│   │   ├── check_expr.c      expressions
│   │   ├── check_stmt.c      statements, blocks and `if`
│   │   ├── check_pattern.c   patterns
│   │   ├── check_match.c     `match` arms and their types
│   │   ├── exhaustive.c      exhaustiveness and unreachable arms (usefulness)
│   │   ├── check_method.c    method calls on primitives (std methods, then built-ins)
│   │   ├── check_user_method.c method calls on user types
│   │   ├── check_box.c       dereferencing boxes and references: `*b`, `b.x`, `b.m()`
│   │   ├── check_clone.c     which types can be cloned, the derived `.clone()` (E0322)
│   │   ├── check_iter.c      `for x in it` over a type with `next(mut self) Option<T>`
│   │   ├── check_ref.c       `&x`, `&mut x`, second-class rules (E0702), returned borrows
│   │   ├── check_list.c      `List<T>`: literals, indexing, `for`, `sort` in stage 0
│   │   ├── check_option.c    `?`, `??`, unused results, the Option/Result layouts
│   │   ├── check_generic.c   type arguments: explicit and inferred
│   │   ├── check_unsafe.c    `unsafe` contexts and raw pointers
│   │   ├── structs.c         struct fields, literals, `@lang` layouts
│   │   ├── enums.c           enum variants and payloads
│   │   ├── const_eval.c/.h   constant evaluation
│   │   ├── builtins.c/.h     the methods still built in until bounds (ADR 0019)
│   │   └── bounds.c          generic bounds and `where` clauses (planned)
│   ├── air/
│   │   ├── air.c/.h          Astra Intermediate Representation
│   │   ├── air_print.c/.h    AIR dump (`--emit=air`)
│   │   ├── air_validate.c    structural checks after lowering
│   │   ├── lower.c/.h        typed AST → AIR: functions reachable from `main`
│   │   ├── lower_internal.h  state shared by the lowering files
│   │   ├── lower_stmt.c      statements and loops
│   │   ├── lower_expr.c      expressions, calls, `if`, `&&`/`||`
│   │   ├── lower_ops.c       checked operators, casts, interpolation, printing
│   │   ├── lower_method.c    method calls, numeric constants, indexing, panics
│   │   ├── lower_intrinsic.c `extern "intrinsic"` calls → AIR operations
│   │   ├── lower_pattern.c   patterns and `match`
│   │   ├── lower_struct.c    struct and enum values
│   │   ├── lower_list.c      list literals, bounds-checked elements, `for` over lists and maps
│   │   ├── lower_hash.c      `hashOf<K>` and `equals<K>` for Map keys
│   │   ├── lower_clone.c     clone glue, the derived `.clone()` and `cloneOf<T>`
│   │   ├── lower_drop.c      drop glue, `dropInPlace<T>` and `forget<T>`
│   │   ├── lower_iter.c      `for x in it`: `next()` until `None`
│   │   ├── lower_scope.c     drop scopes, drop flags (also per moved part), temporaries
│   │   ├── lower_option.c    `?` and `??`
│   │   ├── mono.c            monomorphization: instances, type substitution
│   │   ├── moves.c           move analysis: states, statements, loops (E0600-E0602)
│   │   └── moves_expr.c      move analysis of expressions, places and patterns
│   ├── codegen/
│   │   ├── llvm.c/.h         AIR → textual LLVM IR: module, names, constants
│   │   ├── llvm_internal.h   state shared by the generator files
│   │   ├── llvm_types.c      struct, enum and tuple types and instances, field paths
│   │   ├── llvm_body.c       functions, statements, terminators
│   │   ├── llvm_ops.c        operands and rvalues
│   │   ├── llvm_misc.c       string, float and pointer operations, aggregates
│   │   ├── llvm_cast.c       range checks of `as`
│   │   ├── out.c/.h          buffered text writer
│   │   └── abi.c/.h          type layout and calling convention (planned)
│   ├── driver.c/.h           the front end phases and the command line options
│   ├── driver_modules.c      loading the root file, its imports and the prelude
│   ├── driver_paths.c        finding the installed runtime and std
│   ├── driver_backend.c      lowering, code generation, clang, `run`
│   └── driver_internal.h     state shared by the driver files
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
│   ├── entry.c               process entry, argument capture
│   ├── panic.c               panic reporting and exit
│   ├── mem.c                 global allocator wrapper
│   ├── io.c                  writes and the standard output buffer
│   ├── format.c, str.c       numbers and strings for interpolation
│   ├── os.c                  arguments, environment, exit, running programs (std.os)
│   ├── stack.c               stack overflow detection
│   └── fs.c                  files, directories, standard input (std.fs, std.io)
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
├── core/                     std.core, the prelude: imported into every module (spec 7.4)
│   ├── mod.at                the prelude's own declarations (Ordering) and imports
│   ├── str.at                `@lang("str")`: the methods of str
│   ├── split.at              StrSplit and StrBytes, the iterators of str
│   ├── print.at              eprint and eprintln
│   ├── num/                  i8.at ... u64.at, f32.at, f64.at: the numeric methods, one file per
│   │                         type, generated by tools/gen_core_numbers.py
│   ├── box.at                `@lang("box")`: Box<T>, and the sizeOf<T>() intrinsic
│   ├── list.at               `@lang("list")`: List<T>
│   ├── slice.at              `@lang("slice")`: Slice<T>, the type `&[T]`
│   ├── map.at                `@lang("map")`: Map<K, V>, ordered by insertion
│   ├── hash.at               the deterministic hash functions of Map keys
│   ├── option.at             `@lang("option")`: Option<T>, with Some and None in scope
│   ├── result.at             `@lang("result")`: Result<T, E>, with Ok and Err in scope
│   ├── interfaces.at         Eq, Ord, Hash, Clone, Display, Debug, Iterator, ...
│   └── panic.at
├── mem/                      std.mem: replace and swap; take, UnsafeCell, Cell [planned]
├── collections/              std.collections [planned]: sets, deques; List, Map and Box are in std/core
├── strings/                  std.strings: join, fromUtf8, Builder
├── fmt/                      std.fmt: formatting used by interpolation
├── io/                       std.io: IoError, stdin, the runtime's buffers
├── fs/                       std.fs: files and directories
├── os/                       std.os: args, env, exit, run
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
├── check_sizes.py            size rules of the style guide (make test)
├── check_docs.py             documentation comments of std, ADR 0020 (make test)
├── check_leaks.py            every run test under valgrind (make check-leaks)
├── gen_core_numbers.py       generates std/core/num/
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
- `runtime` must not depend on `std`; `std/core` allocates only through
  the runtime allocator (`astra_alloc`, for `str`), never through collections.
- Types and methods are documented in the [standard library
  reference](../std/README.md).
