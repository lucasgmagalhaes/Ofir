# Bootstrap plan

The goal is a **self-hosted** Astra compiler: the compiler is written in
Astra and compiles itself. A compiler cannot compile itself before it
exists, so we get there in stages.

## Stages

```
stage0  bootstrap compiler, written in a host language
          │ compiles
          ▼
stage1  Astra compiler written in Astra, built by stage0
          │ compiles
          ▼
stage2  Astra compiler built by stage1
          │ compiles
          ▼
stage3  Astra compiler built by stage2   →  must be identical to stage2
```

### Stage 0 — bootstrap compiler

- Written in **C11** (see [ADR 0003](adr/0003-self-hosting-with-c-bootstrap.md)).
- Implements only the subset of Astra needed to write the real compiler
  (see below). Error messages can be basic; optimizations come from LLVM.
- Lives in `bootstrap/`. It is frozen once stage 1 works and is only
  changed when the self-hosted compiler needs a new feature to build itself.

### Stage 1 — self-hosted compiler

- Written in Astra, in `compiler/`.
- Implements the full specification, with high-quality diagnostics.
- Built first by stage 0, then by itself.

### Verification

Stage 2 and stage 3 must produce byte-identical output. This proves the
compiler compiles itself correctly and is checked in CI.

### Retiring stage 0 **[planned]**

Once the self-hosted compiler is stable, stage 0 can be replaced by a
**seed**: the LLVM IR that the compiler generates for itself, checked into
the repository. Building Astra from scratch then needs only `clang`.

## Compiler pipeline

Both compilers share the same architecture, described in
[design/compiler-architecture.md](design/compiler-architecture.md):

```
source (.astra)
  → lexer            tokens
  → parser           AST
  → resolver         names bound to declarations, modules loaded
  → type checker     typed AST
  → lowering         AIR: control-flow graph
  → borrow checker   moves, borrows, definite initialization
  → drops            explicit drops and drop flags
  → mono             concrete instances of generic code
  → codegen          LLVM IR (textual .ll)
  → clang            optimization, machine code, linking with the runtime
```

Generating **textual LLVM IR** instead of linking against the LLVM
libraries keeps the compiler free of C bindings and easy to self-host. The
generated code is optimized by exactly the same LLVM passes either way
(see [ADR 0001](adr/0001-textual-llvm-ir-backend.md)).

## Runtime

A small C library (`runtime/`) provides what LLVM IR cannot express
portably: memory allocation, process entry, panics and system calls. It is
linked into every program and will shrink over time as the standard library
moves to Astra. See [design/runtime.md](design/runtime.md).

## Bootstrap subset

Stage 0 supports only what the self-hosted compiler needs. Until milestone
M4, the code in `compiler/` must be written in this subset.

**Included:**

- functions, `let`/`var`, `const`, all integer types, `float`, `bool`
- arithmetic (checked), comparisons, logic, bitwise operators, `as`
- `if`, `for` (all forms), `break`, `continue`, `return`
- `str`, string interpolation (without format specifiers), `print`/`println`
- structs, methods, associated functions, `copy struct`
- enums with data, `match` (exhaustiveness checked), `if let`
- tuples
- references `&T`, `&mut T` as parameters and locals; slices `&[T]`, `&str`
- built-in generics: `List<T>`, `Map<K, V>`, `Option<T>`, `Result<T, E>`,
  `Box<T>`, and the `?` operator
- moves and automatic drops
- modules, `import`, `pub`
- `extern "C"` blocks and `unsafe` blocks
- `@test` functions

**Excluded** (implemented only in stage 1):

- the borrow checker (stage 0 checks moves, not borrows)
- user-defined generics, interfaces and `dyn`
- closures
- `ref struct` and returning borrows
- format specifiers, raw strings, attributes other than `@test`
- good diagnostics: stage 0 reports the first error per function

Programs accepted by stage 0 are still valid Astra: stage 1 enforces every
rule when it compiles itself.

## Milestones

| # | Milestone | Done when |
|---|-----------|-----------|
| M0 | Specification draft | `docs/spec` reviewed |
| M1 | Hello world | stage 0 compiles and runs `examples/hello.astra` |
| M2 | Bootstrap subset | stage 0 passes every test in `tests/` not marked `requires: stage1` |
| M3 | Self-hosted front end | stage 1 lexes and parses its own source |
| M4 | Self-hosted | stage 1 compiles itself and stage2 == stage3 |
| M5 | Full language | every chapter of the specification is implemented |

The detailed task list is in [roadmap.md](roadmap.md).
