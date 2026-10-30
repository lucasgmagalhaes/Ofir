# Roadmap

This roadmap turns the specification and design documents into an ordered
list of small, verifiable tasks. Each task should fit in one pull request
made of small Conventional Commits, and is done only when its tests pass in
CI.

## Strategy

1. **Walking skeleton first.** Get `hello world` working end to end through
   every phase (lexer → clang) as early as possible, then grow the language
   one feature at a time *through all phases*. This finds integration
   problems early and always keeps a working compiler.
2. **Tests before features.** Each feature starts with its `tests/run` and
   `tests/compile_fail` cases.
3. **Stage 0 stays minimal.** Anything not needed to build the self-hosted
   compiler goes to stage 1.
4. **Port, then extend.** Stage 1 first reproduces stage 0 exactly (same
   test results), then adds the full language.

Size legend: **S** ≈ a few hours, **M** ≈ a few days, **L** ≈ one to two
weeks of focused work.

## Overview

```
Phase 0  Foundations            repository, build, CI, test runner
Phase 1  Walking skeleton       hello world through all phases        → M1
Phase 2  Stage 0 core language  integers → structs → enums → lists
Phase 3  Stage 0 systems        ownership, modules, FFI, std subset   → M2
Phase 4  Stage 1 port           the compiler rewritten in Astra       → M3, M4
Phase 5  Full language          borrow checker, generics, interfaces  → M5
Phase 6  Ecosystem              formatter, LSP, packages, docs        → v1.0
```

---

## Phase 0 — Foundations

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 0.1 | Create the directory skeleton of `docs/design/project-structure.md` with a `README.md` per top-level directory | S | directories exist and describe their purpose |
| 0.2 | Add `LICENSE`, `CONTRIBUTING.md` (Conventional Commits, branch and PR rules), `CHANGELOG.md`, `.editorconfig`, `.gitignore` | S | files present |
| 0.3 | Top-level `Makefile` with targets `stage0`, `runtime`, `test`, `clean` (empty implementations allowed) | S | `make` runs without errors |
| 0.4 | `bootstrap/Makefile`: C11, `-Wall -Wextra -Werror`, debug build with `-fsanitize=address,undefined` | S | an empty `main.c` builds |
| 0.5 | Test runner `tests/runner/run_tests.py`: discovery, `run`, `compile_fail`, `panic` suites, `--bless`, parallel execution | M | runner self-test passes on fixture files |
| 0.6 | CI workflow: build stage 0 and runtime, run all suites on Linux and macOS | S | green pipeline on the main branch |

## Phase 1 — Walking skeleton (milestone M1)

Goal: `astra run examples/hello.at` prints `Hello, Astra!`.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 1.1 | `base/`: arena allocator, dynamic array macros, string builder, string views | M | C unit tests pass |
| 1.2 | `base/`: hash map and identifier interning | S | C unit tests pass |
| 1.3 | `base/`: file reading, path joining; file table with `FileId` and spans | S | C unit tests pass |
| 1.4 | `diag/`: diagnostic collection and rendering with source excerpt and caret | S | output matches design doc format |
| 1.5 | Lexer: all tokens of spec chapter 1 except interpolation and raw strings; `NL` insertion | M | `tests/lex` cases pass, `--emit=tokens` works |
| 1.6 | Lexer: string interpolation tokens | S | `tests/lex/interpolation*` pass |
| 1.7 | AST definitions and AST printer (`--emit=ast`) | M | printer covers every node kind |
| 1.8 | Parser: items (`fn`, `struct`, `enum`, `const`, `import`), types, statements | M | `tests/parse` cases pass |
| 1.9 | Parser: expressions with precedence, struct literal restriction, patterns | M | `tests/parse` cases pass |
| 1.10 | Parser: error recovery to next statement | S | several syntax errors reported per file |
| 1.11 | Types: interned type table with primitive types, `str`, `()` | S | unit tests |
| 1.12 | Resolver: single module, functions and locals | S | unknown-name errors reported |
| 1.13 | Type checker: functions, `let`/`var`, integer literals, calls, `println` with interpolation of `str` and `int` | M | `tests/compile_fail/types_basic*` pass |
| 1.14 | AIR definitions, printer (`--emit=air`) and validator | M | AIR dumps for hello world |
| 1.15 | Lowering for the skeleton subset | S | AIR validator passes |
| 1.16 | Codegen: functions, locals, integer arithmetic, calls, string constants | M | generated `.ll` accepted by clang |
| 1.17 | Runtime: entry point, `astra_write`, panic functions, allocator | S | runtime builds with `-Werror` |
| 1.18 | Driver: `astra build`/`run` for a single file, invoking clang, `--emit=*` flags | S | **M1: hello world runs** |

## Phase 2 — Stage 0 core language

Each task covers lexer → parser → checker → lowering → codegen → tests.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 2.1 | All integer types, `float`, `bool`; `as` conversions with checks | M | `tests/run/numbers/*` and `tests/panic/cast*` pass |
| 2.2 | Checked arithmetic (`llvm.*.with.overflow`), division by zero, shifts | S | `tests/panic/overflow*` pass |
| 2.3 | Comparison, logic (short-circuit), bitwise operators | S | `tests/run/operators/*` pass |
| 2.4 | `if`/`else` as statement and expression | S | `tests/run/control/if*` pass |
| 2.5 | `for` (infinite, condition, ranges), `break`, `continue` | S | `examples/fibonacci.at` runs |
| 2.6 | Missing-return analysis, `!` type, `panic`, `assert` | S | `tests/compile_fail/missing_return*` pass |
| 2.7 | `const` declarations with compile-time evaluation | S | tests pass |
| 2.8 | `str`: literals, `+`, `==`, comparison, `len`, indexing, `push` | M | `tests/run/strings/*` pass |
| 2.9 | Interpolation of all primitive types (`Display` for primitives) | S | float formatting matches spec |
| 2.10 | Structs: declaration, literals, field access and assignment, nested structs, `copy struct` | M | `tests/run/structs/*` pass |
| 2.11 | Methods and associated functions; auto-referencing of receivers | M | `tests/run/methods/*` pass |
| 2.12 | References `&T`/`&mut T` as parameters and locals, auto-deref, `*` | M | `tests/run/refs/*` pass |
| 2.13 | Tuples and destructuring `let` | S | tests pass |
| 2.14 | Enums: layout, construction, `.Variant` shorthand | M | tests pass |
| 2.15 | `match`: decision trees, literal, range, variant, tuple, struct patterns, guards | L | `examples/shapes.at` runs |
| 2.16 | Exhaustiveness checking (usefulness algorithm) | M | `tests/compile_fail/match_*` pass |
| 2.17 | `if let` | S | tests pass |
| 2.18 | `Box<T>` and recursive enums | M | expression tree test program runs |
| 2.19 | `List<T>`: literals, `push`, `pop`, `len`, indexing with bounds checks, `for` over lists | L | `tests/run/list/*`, `tests/panic/bounds*` pass |
| 2.20 | Slices `&[T]` and `&str`, range indexing | M | tests pass |
| 2.21 | `Option<T>`, `Result<T, E>`, `?`, `unwrap`, `expect`, unused-`Result` error | M | `tests/run/errors/*` pass |
| 2.22 | `Map<K, V>` (open addressing, hashing of integers and `str`) | L | `tests/run/map/*` pass |

## Phase 3 — Stage 0 systems features (milestone M2)

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 3.1 | Move analysis on AIR: use after move, conditional moves, definite initialization | L | `tests/compile_fail/moves*` pass |
| 3.2 | Drop elaboration, drop flags, drop glue, user `drop` methods | L | leak-free under ASan for all `tests/run` |
| 3.3 | `.clone()` for built-in types and derived for structs/enums | M | tests pass |
| 3.4 | Modules: multiple files, `import`, aliases, `pub`, module-qualified names | L | multi-file tests pass |
| 3.5 | Prelude and standard library loading from `std/` | M | `Option`/`Result` helpers live in `std/core` |
| 3.6 | `extern "C"` blocks, `unsafe` blocks, raw pointers | M | calling `strlen` works |
| 3.7 | `std.io` (stdout, stderr, buffered writer), `std.fs` (read/write files) | M | `examples/word_count.at` runs |
| 3.8 | `std.os`: args, env, exit, spawn process and wait | M | a test program runs `clang --version` |
| 3.9 | `std.strings` helpers: split, trim, find, starts_with, parse integers | M | tests pass |
| 3.10 | `@test` functions and `astra test` | M | `std` tests run in CI |
| 3.11 | Freeze review: run all suites under ASan/UBSan, fix leaks and crashes | M | **M2: stage 0 complete** |

## Phase 4 — Self-hosting (milestones M3 and M4)

Stage 1 is written in the bootstrap subset and mirrors stage 0's structure.
Each port is verified by comparing outputs with stage 0.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 4.1 | `compiler/base`: typed index vectors, interning, spans | M | `@test`s pass |
| 4.2 | `compiler/diag` | S | rendering identical to stage 0 |
| 4.3 | `compiler/syntax/lexer` | M | `--emit=tokens` identical to stage 0 on all tests |
| 4.4 | `compiler/syntax/parser` + AST printer | L | `--emit=ast` identical to stage 0 on all tests; **M3** when it parses `compiler/` itself |
| 4.5 | Resolver and module loading | M | resolve errors identical |
| 4.6 | Type checker | L | `tests/compile_fail` pass |
| 4.7 | AIR, lowering, move analysis, drop elaboration | L | `--emit=air` equivalent to stage 0 |
| 4.8 | LLVM codegen and layout | L | `tests/run` and `tests/panic` pass with stage 1 built by stage 0 |
| 4.9 | Driver and CLI | M | `astra build/run/test` work |
| 4.10 | Stage 1 builds itself (stage 2), stage 2 builds itself (stage 3) | M | stage 2 and stage 3 `.ll` outputs are identical |
| 4.11 | `make verify` in CI | S | **M4: self-hosted** |

From M4 on, new features are implemented only in stage 1. Stage 0 is
frozen, and `compiler/` may use features as soon as stage 1 supports them
(compiling itself with the previous stage 1 release).

## Phase 5 — Full language (milestone M5)

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 5.1 | Borrow checker: loans, liveness, conflicts (spec 5.5) | L | `tests/borrow/*` pass |
| 5.2 | Escape rules for second-class references (spec 5.4) | M | tests pass |
| 5.3 | Returning borrows and `ref struct` (spec 5.6) | M | `List.iter()` written in Astra |
| 5.4 | User-defined generic functions and types, bounds, `where` | L | `tests/run/generics/*` pass |
| 5.5 | Monomorphization with symbol mangling and recursion limit | M | tests pass |
| 5.6 | Interfaces: declaration, structural satisfaction, default methods, embedding | L | `tests/run/interfaces/*` pass |
| 5.7 | `dyn`: vtables, fat pointers, type switches | M | heterogeneous list example runs |
| 5.8 | Built-in interfaces derived automatically (`Eq`, `Ord`, `Hash`, `Clone`, `Debug`) | M | `==` on structs, structs as map keys |
| 5.9 | Rewrite `List`, `Map`, `Option`, `Result` as generic Astra code in `std/` | L | built-in special cases removed from the compiler |
| 5.10 | Closures: captures, non-escaping on stack, escaping with heap environment | L | `tests/run/closures/*` pass |
| 5.11 | Iterator interface and `for` over iterators | M | tests pass |
| 5.12 | Format specifiers, raw strings, multi-line strings | M | tests pass |
| 5.13 | Attributes: `@inline`, `@cold`, `@repr(C)`, `@cfg`, `@deprecated`, `@export`, `@link` | M | tests pass |
| 5.14 | Diagnostics: error codes, notes and help messages, `astra explain`, JSON output | M | every error has a code and a documented explanation |
| 5.15 | Debug info (DWARF) | M | stepping through code in `lldb` works |
| 5.16 | Optimization: `noalias`/`readonly` attributes, niche layouts, field reordering | M | benchmarks within 10% of C |
| 5.17 | Benchmarks suite with C/Rust/Go baselines in CI | M | results published per release |

**M5** is reached when every chapter of the specification not marked
**[planned]** is implemented and tested.

## Phase 6 — Ecosystem (towards v1.0)

| # | Task | Size |
|---|------|:----:|
| 6.1 | `astra fmt` (formatter sharing the parser and AST printer) | M |
| 6.2 | Language server (diagnostics, go to definition, completion) | L |
| 6.3 | `astra doc` and doc tests | M |
| 6.4 | Package manager: `astra.toml` dependencies, lock file, registry design | L |
| 6.5 | Incremental and parallel compilation | L |
| 6.6 | Concurrency (spec chapter 14): threads, channels, `Send`/`Sync` | L |
| 6.7 | Windows support | L |
| 6.8 | Seed-based bootstrap replacing stage 0 | M |
| 6.9 | Website, tutorial ("The Astra Book"), playground | L |

## Releases

| Version | Content |
|---------|---------|
| 0.1 | M2: stage 0 compiler usable for small programs |
| 0.2 | M4: self-hosted compiler |
| 0.5 | M5: full language |
| 1.0 | Ecosystem tools, stability guarantee for the language and `std` |

## Open questions

These do not block Phase 0–3 and will be settled with an ADR when needed:

1. License (MIT, Apache-2.0 or dual).
2. Exact `Map` iteration order guarantee (insertion order or unspecified).
3. Whether `str` indexing by byte stays, or iteration by Unicode scalar is
   the default.
4. Package registry hosting and naming rules.
5. Async or green threads on top of the thread model.

## Next steps

1. Review and accept the specification (M0).
2. Start Phase 0: tasks 0.1 to 0.6.
