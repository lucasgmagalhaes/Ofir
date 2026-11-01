# Roadmap

This roadmap turns the specification and design documents into an ordered
list of small, verifiable tasks. Each task should fit in one pull request
made of small Conventional Commits, and is done only when its tests pass in
CI.

## Strategy

1. **Validate before building.** Before writing stage 0, part of the future
   compiler is written on paper in the bootstrap subset, to confirm the
   subset (and second-class references) are expressive enough.
2. **Walking skeleton first.** Get `hello world` working end to end through
   every phase (lexer → clang) as early as possible, then grow the language
   one feature at a time *through all phases*.
3. **Tests before features.** Each feature starts with its `tests/run` and
   `tests/compile_fail` cases.
4. **Stage 0 stays minimal.** Anything not needed to build the self-hosted
   compiler goes to stage 1.
5. **Port, then extend.** Stage 1 first reproduces stage 0 (same test
   results), then gains the borrow checker (required for M4), then the full
   language.

Size legend: **S** ≈ a few hours, **M** ≈ a few days, **L** ≈ one to two
weeks of focused work.

## Overview

```
Phase 0  Foundations            decisions, repository, CI, test runner, paper prototype
Phase 1  Walking skeleton       hello world through all phases           → M1
Phase 2  Stage 0 core language  numbers → structs → enums → collections
Phase 3  Stage 0 systems        ownership, modules, FFI, std subset      → M2
Phase 4  Self-hosting           compiler in Astra + borrow checker       → M3, M4
Phase 5  Full language          generics, interfaces, closures, perf     → M5
Phase 6  Towards 1.0            concurrency, tools, packages, docs       → v1.0
```

## Decisions needed before Phase 0

| Decision | Blocks | Status |
|----------|--------|--------|
| License | 0.2 | decided: **MIT** |

Everything else needed for Phases 0–3 is decided in the specification and
the ADRs.

---

## Phase 0 — Foundations

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 0.1 | Create the directory skeleton of `docs/design/project-structure.md` with a `README.md` per top-level directory | S | **done**: directories exist and describe their purpose |
| 0.2 | Add `LICENSE`, `CONTRIBUTING.md` (Conventional Commits, branches, PRs, ADR process), `CHANGELOG.md`, `.editorconfig`, `.gitignore` | S | **done**: files present |
| 0.3 | Top-level `Makefile` with targets `stage0`, `runtime`, `test`, `clean` (empty implementations allowed) | S | **done**: `make` runs without errors |
| 0.4 | `bootstrap/Makefile`: C11, `-Wall -Wextra -Werror`, debug build with `-fsanitize=address,undefined` | S | **done**: an empty `main.c` builds |
| 0.5 | Test runner `tests/runner/run_tests.py`: discovery, `run`, `compile_fail` (stage 0 and stage 1 matching rules), `panic` suites, `// requires:` tags, `--bless`, parallel execution | M | **done**: runner self-test passes on fixture files |
| 0.6 | Script checking that every error code used in tests exists in `docs/errors/README.md` | S | **done**: runs in CI |
| 0.7 | CI workflow: build stage 0 and runtime, run all suites on Linux and macOS with the oldest and newest supported LLVM | S | **done**: green pipeline on the main branch |
| 0.8 | **Paper prototype:** write the stage 1 lexer (`compiler/syntax/lexer.at`, ~500 lines) in the bootstrap subset, without compiling it; record every missing feature or awkward pattern | M | done: see [prototype review](design/prototype-review.md) |

## Phase 1 — Walking skeleton (milestone M1)

Goal: `astra run examples/hello.at` prints `Hello, Astra!`.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 1.1 | `base/`: arena allocator, dynamic array macros, string builder, string views | M | **done**: C unit tests pass |
| 1.2 | `base/`: hash map (insertion ordered) and identifier interning | S | **done** (interning; the general map is added when first needed): C unit tests pass |
| 1.3 | `base/`: file reading, path joining; file table with `FileId` and spans | S | **done**: C unit tests pass |
| 1.4 | `diag/`: diagnostics with codes, rendering with source excerpt and caret | S | **done**: output matches the design doc format |
| 1.5 | Lexer: all tokens of spec chapter 1 except interpolation, raw and byte strings; newline tokens; contextual keywords as identifiers | M | **done**: `tests/lex` cases pass, `--emit=tokens` works |
| 1.6 | Lexer: string interpolation tokens | S | **done**: `tests/lex/interpolation*` pass |
| 1.7 | AST definitions and AST printer (`--emit=ast`) | M | **done**: printer covers every node kind |
| 1.8 | Parser: items (`fn`, `struct`, `enum`, `const`, `import`), types, statements, parser-driven newline handling (spec 1.3) | M | **done**: `tests/parse` cases pass, including `tests/parse/generics_and_newlines` |
| 1.9 | Parser: expressions with precedence, struct literal restriction, generic disambiguation, patterns | M | **done**: `tests/parse` cases pass |
| 1.10 | Parser: error recovery to next statement | S | **done**: several syntax errors reported per file (`tests/compile_fail/parser/recovery`) |
| 1.11 | Types: interned type table with primitive types, `str`, `()` | S | **done**: unit tests (`bootstrap/tests/test_types.c`) |
| 1.12 | Resolver: single module, functions and locals | S | **done**: `E0200`/`E0201` reported (`tests/compile_fail/names`) |
| 1.13 | Type checker: functions, `let`/`var`, integer literals, calls, `println(str)`, literals passed to `str` parameters, interpolation of `str` and `int` | M | **done**: `tests/compile_fail/types/*` pass, also in stage 1 mode |
| 1.14 | AIR definitions, printer (`--emit=air`) and validator | M | **done**: AIR dumps for hello world |
| 1.15 | Lowering for the skeleton subset | S | **done**: AIR validator passes |
| 1.16 | Codegen: functions, locals, integer arithmetic, calls, string constants; no `noalias` | M | **done**: generated `.ll` accepted by clang (CI: 17 and the latest) |
| 1.17 | Runtime: entry point, stdout buffer, `astra_write`, panic functions, allocator | S | **done**: runtime builds with `-Werror` |
| 1.17a | **done**: Benchmark harness: `bench/run_bench.py`, `--time-passes`, `--stop-after`, CI regression gate | S | `make bench` and `make bench-compare` work |
| 1.17b | Benchmarks for each new phase (`parse-100k`, `check-100k`, `build-100k`) added with the phase | S | **done**: `parse-*`, `check-*` and `llvm-*` (IR generation without clang) |
| 1.18 | Driver: `astra build`/`run` for a single file, invoking clang, `--emit=*` flags | S | **done — M1: hello world runs**; binary size: 17 KiB (14.6 KiB stripped) |

## Phase 2 — Stage 0 core language

Each task covers lexer → parser → checker → lowering → codegen → tests.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 2.1 | All integer types, `float`, `bool`; `as` conversions with checks; `wrappingCast`/`saturatingCast`/`checkedCast` | M | **done**: `tests/run/numbers/*`, `tests/panic/cast*` pass; `checkedCast` since 2.22 |
| 2.2 | Checked arithmetic (`llvm.*.with.overflow`), division semantics, shifts | S | **done**: `tests/panic/*overflow*`, `div_*`, `shift` pass |
| 2.3 | Comparison, logic (short-circuit), bitwise operators | S | **done**: covered by `tests/run/basics/*` |
| 2.4 | `if`/`else` as statement and expression | S | **done**: `tests/run/basics/control` |
| 2.5 | `for` (infinite, condition, ranges), `break`, `continue` | S | **done**: `examples/fibonacci.at` runs |
| 2.6 | Missing-return analysis, `!` type, `panic`, `assert` | S | **done**: `tests/compile_fail/types/returns`, `tests/panic/assert`, `explicit` |
| 2.7 | `const` declarations with compile-time evaluation | S | **done**: `tests/run/basics/constants`, `tests/compile_fail/types/constants` |
| 2.8 | `str`: literals, `+`, `+=`, `==`, comparison, `len`, byte indexing, `push` | M | **done**: `tests/run/strings/*` pass |
| 2.9 | Interpolation of all primitive types; shortest round-trip float formatting (spec 2.1) | M | **done**: `tests/run/basics/strings`, `numbers/methods` |
| 2.10 | Structs: declaration, literals, field access and assignment, nested structs, `copy struct`, default field values, tuple structs | M | **done**: `tests/run/structs/*` pass |
| 2.11 | Methods and associated functions declared inside structs and enums, `self` | M | **done** for structs (enums with 2.14): `tests/run/methods/*` pass |
| 2.12 | Parameter modes (borrow, `mut`, `move`) for parameters and `self`, `for mut`, `[T]`/`str` view parameters; explicit `&T`/`&mut T` locals, auto-deref, `*` | M | **in progress**: modes of parameters and `self` done (`mut` by reference); `for mut` done; views, `&T` locals and `*` remain |
| 2.13 | Tuples and destructuring `let` | S | **done**: `tests/run/enums/patterns` |
| 2.14 | Enums: layout, construction, `.Variant` shorthand | M | **done**: `tests/run/enums/*` |
| 2.15 | `match`: decision trees, literal, range, variant, tuple, struct patterns, guards; Copy bindings through references | L | **done** with sequential arm tests (decision trees later, if profiles ask for them); `examples/shapes.at` runs since `List` (2.20) |
| 2.16 | Exhaustiveness checking (usefulness algorithm) | M | **done**: `sema/exhaustive.c`; errors show an uncovered value (`Err(.TooLarge)`), unreachable arms warn (W0004); `tests/compile_fail/types/exhaustive.at`, `tests/run/enums/exhaustive.at` |
| 2.17 | `if let`, `let ... else`, `??` | S | **done**: `tests/run/enums/if_let`; `??` since 2.22 (`tests/run/errors/option.at`) |
| 2.18 | Generic functions, structs and enums **without bounds**; monomorphization and mangling | L | **done**: `tests/run/generics_basic/*`, `tests/compile_fail/types/generics*`; type arguments are inferred from the receiver, the expected type and the arguments |
| 2.19 | `Box<T>` and recursive enums | M | **done**: `tests/run/box/expr_tree.at`; `Box` is written in `std/core/box.at` with `@lang("box")` and the `sizeOf<T>()` intrinsic; a box is freed by `unbox` until drops (3.2) |
| 2.20 | `List<T>` (std/collections.md S0 entries): literals, bounds checks, `for` over lists with Copy loop variables, `sort` | L | **done**: `tests/run/list/*`, `tests/panic/bounds_list*`; written in `std/core/list.at` (`@lang("list")`); `pop`, `get`, `first`, `last` wait for `Option` and borrows (2.21, 2.22), `contains` and `join` for bounds; `sort` takes int, float and str |
| 2.21 | Slices `&[T]` and `&str`, range indexing, built-in borrow-returning methods (`get`, `first`, `last`, `getMut`) | M | **done**: `tests/run/refs/*`, `tests/compile_fail/types/references.at`; `&T`, `&mut T` and the second-class rules (E0702); `Slice<T>` in `std/core/slice.at`; borrows are not checked until the borrow checker (4.7); `&str` is a `str` view, `&mut [T]` and `Option.asRef` wait |
| 2.22 | `Option<T>`, `Result<T, E>`, `?` (equal error types), `unwrap`, `expect`, unused-`Result` error | M | **done**: `tests/run/errors/*`; written in `std/core/option.at` and `result.at` (`@lang("option")`, `@lang("result")`); `Some`, `None`, `Ok`, `Err` in scope everywhere; `List.pop`; `asRef` waits for references (2.21) |
| 2.23 | `Map<K, V>`: insertion order, deterministic hashing of integers, `str` and tuples | L | **done**: `tests/run/map/*`; written in `std/core/map.at` and `hash.at`, keys hashed through the `hashOf`/`equals` intrinsics; `for key, value in map` and `for i, x in list` |
| 2.24 | Warnings: unused variables, unused imports, unreachable code; `--deny-warnings` | S | **done**: `tests/compile_fail/warn_*`; W0001 exempts parameters and names starting with `_`; `run` tests use `--deny-warnings` |

## Phase 2b — Standard library in Astra (ADR 0019)

Done before the rest of phase 2, so that library functions are written in
Astra instead of being built into the compiler.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 2b.1 | Modules (task 3.4): several files, `import`, aliases, `pub`, qualified names | L | **done**: `tests/run/modules/*` |
| 2b.2 | Prelude: `std/core` loaded in every compilation, its `pub` names visible everywhere (task 3.5) | M | **done**: `Ordering` in `std/core/index.at` |
| 2b.3 | `extern "C"`, `unsafe` blocks and raw pointers `*T` (task 3.6) | M | **done**: `tests/run/ffi/libc.at` |
| 2b.4 | `@lang` declarations of primitive types and `extern "intrinsic"` functions | M | **done**: `std/core/str.at` |
| 2b.5 | Move the built-in methods of `str`, integers and floats to `std/core`; remove `sema/builtins.c`; shrink the runtime to entry, panics, allocation and I/O | L | **done**: only `wrappingCast`, `saturatingCast`, `p.add` and the numeric constants stay built in until generic bounds (5.2); the integer methods are generated by `tools/gen_core_numbers.py` |

Then phase 2 resumes with generics (2.18, done), so that `Box`, `List`,
`Option` and `Result` are written in `std` from the start (task 5.6 moves
earlier accordingly).

## Phase 3 — Stage 0 systems features (milestone M2)

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 3.1 | Move analysis on AIR: use after move, conditional moves, moves out of borrowed or indexed places, definite initialization | L | **done** on the typed AST (`sema/moves*.c`): `tests/compile_fail/moves.at`, `tests/run/moves/*`; `std.mem.replace` and `swap`; Map removes entries instead of marking them dead |
| 3.2 | Drop elaboration, drop flags, drop glue, user `drop` methods | L | **done**: `make check-leaks` (valgrind, as clang here has no ASan runtime) finds no leak or invalid access in `tests/run` and `examples`; `tests/run/drops/order.at`; the unbound parts of a value matched by move are not dropped yet |
| 3.3 | `.clone()` for built-in types and derived for structs/enums | M | **done**: clone glue `clone<T>` (`air/lower_clone.c`), the derived clone and E0322 (`sema/check_clone.c`), `clone` in std str, List and Box; `tests/run/clone`, `tests/compile_fail/types/clone.at` |
| 3.4 | Modules: multiple files, `index.at`, `import`, aliases, `pub`, module-qualified names | L | multi-file tests pass |
| 3.5 | Prelude and standard library loading from `std/` | M | `Option`/`Result` helpers live in `std/core` |
| 3.6 | `extern "C"` blocks, `unsafe` blocks, raw pointers | M | calling `strlen` works |
| 3.7 | `std.strings` and `str` S0 methods, including the borrowing iterators `split`, `splitWhitespace`, `lines`, `bytes` | M | **done**: `for x in it` over any type with `fn next(mut self) Option<T>` (`sema/check_iter.c`, `air/lower_iter.c`); `find`, `trim*`, `replace`, `pushChar`, `withCapacity` in `std/core/str.at`, the iterators in `std/core/split.at`; `std.strings` `join`, `fromUtf8`, `Builder`; `tests/run/strings/*`, `tests/run/iterators/*` |
| 3.8 | `std.io` and `std.fs` S0 entries, `IoError` | M | **done**: `examples/word_count.at` runs; runtime `fs.c`, `std/io`, `std/fs`, `eprint`/`eprintln`; `tests/run/system/fs.at`; `main` still returns nothing |
| 3.9 | `std.os` S0 entries: args, env, exit, `run` | M | **done**: `tests/run/system/os.at` runs `clang --version`; runtime `os.c` (`posix_spawnp`, both streams captured) |
| 3.10 | Runtime: stack overflow detection | S | **done**: `runtime/src/stack.c` (`sigaltstack` + `SIGSEGV`/`SIGBUS` handler); `tests/panic/stack_overflow*` pass |
| 3.11 | `@test` functions and `astra test` | M | **done**: `astra test` (`air/lower_test.c`, `driver_test.c`), one process per test; `make test-std` (`tools/run_std_tests.py`) in `make test`; `tests/astra_test/` |
| 3.11a | First generated-code benchmarks (`fib`, `nbody`, `binary_trees`) against C | M | **done**: `bench/run_programs.py`; `binary_trees` 1.01, `nbody` 1.04, `fib` 1.80 (overflow checks; 1.0 with wrapping arithmetic), in `bench/HISTORY.md` |
| 3.12 | Freeze review: all suites under ASan/UBSan, fix leaks and crashes | M | **done**: `make check-asan` (69 programs clean), `make check-leaks` (47 clean), `tools/fuzz_frontend.py` (2 600 mutated inputs, no crash); **M2: stage 0 complete** → release 0.1 |

## Phase 4 — Self-hosting (milestones M3 and M4)

Stage 1 is written in the bootstrap subset and mirrors stage 0's structure.
Each port is verified by comparing outputs with stage 0.

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 4.1 | `compiler/base`: generic typed index vectors, interning, spans | M | **done**: `base.span` (spans, `SourceMap`, line/column), `base.intern`, `base.index_vec` (`Id<T>`, `IndexVec<T>`), 8 tests in `make test-std`; `compiler/astra.toml` makes `compiler/` the import root |
| 4.2 | `compiler/diag` | S | **done**: `diag` (`Diagnostics`, human and short formats, sorted and deduplicated as stage 0 does); tests compare with stage 0's output |
| 4.3 | `compiler/syntax/lexer` (starting from the 0.8 prototype) | M | **done**: `main tokens <file>` identical to stage 0's `--emit=tokens` (output, errors, status) on every `.at` file of the repository and 200 mutated ones (`make compare-stages`, `tools/compare_stages.py`); `checkedAdd/Sub/Mul/Div` and `parse` for the numeric types in std |
| 4.4 | `compiler/syntax/parser` + AST printer | L | `--emit=ast` identical to stage 0; **M3** when it parses `compiler/` itself |
| 4.5 | Resolver and module loading | M | resolve errors identical |
| 4.6 | Type checker | L | `tests/compile_fail` pass |
| 4.7 | AIR, lowering, move analysis, drop elaboration, monomorphization | L | `--emit=air` equivalent to stage 0 |
| 4.8 | LLVM codegen and layout | L | `tests/run` and `tests/panic` pass with stage 1 built by stage 0 |
| 4.9 | Driver and CLI | M | `astra build/run/test` work |
| 4.10 | Borrow checker: loans, liveness, conflicts (spec 5.5) | L | `tests/borrow/*` pass |
| 4.11 | Escape rules for references and borrowed function values (spec 5.4, 10.2) | M | tests pass |
| 4.12 | Fix the compiler's own code until it passes its borrow checker | M | `compiler/` compiles with no borrow errors |
| 4.13 | Stage 1 builds itself (stage 2), stage 2 builds itself (stage 3) | M | stage 2 and stage 3 `.ll` outputs are identical |
| 4.14 | `make verify` in CI; publish the first seed (bootstrap.md) | S | **M4: self-hosted** → release 0.2 |

From M4 on, new features are implemented only in stage 1. Stage 0 is
frozen. `compiler/` may use a feature only once it has shipped in a tagged
release (bootstrap chain, `docs/bootstrap.md`).

## Phase 5 — Full language (milestone M5)

| # | Task | Size | Done when |
|---|------|:----:|-----------|
| 5.1 | Returning borrows and `ref struct` (spec 5.6) | M | `List.iter()` written in Astra |
| 5.2 | Generic bounds and `where` clauses | M | `tests/run/generics/*` pass |
| 5.3 | Interfaces: declaration, structural satisfaction, assertions, default methods, embedding | L | `tests/run/interfaces/*` pass |
| 5.4 | `dyn`: vtables, fat pointers, type switches, unsized types | M | `examples/interfaces.at` runs |
| 5.5 | Built-in interfaces derived automatically (`Eq`, `Ord`, `Hash`, `Clone`, `Debug`); `From` in `?` | M | `==` on structs, structs as map keys |
| 5.6 | Rewrite `List`, `Map`, `Option`, `Result`, `Box` as generic Astra code in `std/` | L | built-in special cases removed from the compiler |
| 5.7 | Closures: `fn` and `move fn` types, captures, stack and heap environments | L | `tests/run/closures/*` pass |
| 5.8 | Iterator interface and `for` over iterators | M | tests pass |
| 5.9 | Interior mutability: `UnsafeCell`, `Cell` | S | tests pass |
| 5.10 | Format specifiers, raw strings, multi-line strings, byte strings | M | tests pass |
| 5.11 | Attributes: `@inline`, `@cold`, `@repr(C)`, `@cfg`, `@deprecated`, `@export`, `@link`, `@allow` | M | tests pass |
| 5.12a | Default parameter values and named arguments | M | `tests/run/calls/*` pass |
| 5.12 | Diagnostics: notes and help messages, `astra explain`, JSON output; an explanation file for every registry code | M | every error code has `docs/errors/EXXXX.md` |
| 5.13 | Debug info (DWARF) | M | stepping through code in `lldb` works |
| 5.14 | Optimization: `noalias`/`readonly` (stage 1 only), niche layouts, field reordering, AIR check elimination | M | benchmarks within 10% of C |
| 5.15 | Generated-code benchmark suite with C/Rust/Go baselines (started at M2 with the first programs) | M | results in `bench/HISTORY.md` per release |

**M5** is reached when every chapter of the specification not marked
**[planned]** is implemented and tested → release 0.5.

## Phase 6 — Towards 1.0

| # | Task | Size |
|---|------|:----:|
| 6.1 | Concurrency (spec chapter 14): threads, channels, `Send`/`Sync`, `std.sync` | L |
| 6.2 | `astra fmt` implementing the [style guide](style-guide.md) | M |
| 6.3 | Language server (diagnostics, go to definition, completion) | L |
| 6.4 | `astra doc` and doc tests | M |
| 6.5 | Package manager: `astra.toml` dependencies, lock file, registry design | L |
| 6.6 | Incremental and parallel compilation | L |
| 6.7 | Seeded `Map` hasher, labeled loops, const generics | M |
| 6.7a | Compile-time code: `comptime`, `std.meta`, `comptime for`/`if`, `@tag` (spec chapter 15) | L |
| 6.8 | Windows support | L |
| 6.9 | Scheduled CI job for the auditable bootstrap path | S |
| 6.10 | Website, tutorial ("The Astra Book"), playground | L |

## Releases

| Version | Content |
|---------|---------|
| 0.1 | M2: stage 0 compiler usable for small programs |
| 0.2 | M4: self-hosted compiler with borrow checker; first seed |
| 0.5 | M5: full language |
| 1.0 | concurrency, formatter, language server, package manager; stability guarantee for the language and `std` |

## Open questions

These do not block Phases 0–4 and will be settled with an ADR when needed:

1. Lazy iterator chains (`list.iter().map(f).filter(g)`) versus eager
   `List.map`/`filter` only (before task 5.8).
2. More patterns: slices (`[first, ..rest]`), strings, `name @ pattern`
   bindings (before task 2.15 is extended in stage 1).
3. Labeled `break`/`continue` (task 6.7).
4. Error handling conventions: error context and cause chains in `std`.
5. 128-bit integers.
6. Concurrency details beyond threads and channels (async, green threads).
7. Packages: registry hosting, versioning, `pub import` re-exports.
8. Custom allocators and arenas in `std`.
9. Advanced FFI: callbacks from C, variadic C functions, `union`.
10. Stability policy and editions.
11. Final project name and repository rename (risk R9).

## Next steps

1. `Map<K, V>` (2.23) in `std`, then the warnings of 2.24, which close
   phase 2.

See also the [risk register](risks.md).
