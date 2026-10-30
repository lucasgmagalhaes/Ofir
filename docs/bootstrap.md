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

- Written in a host language (**decision pending**: C or Rust).
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

Both compilers share the same architecture:

```
source (.astra)
  → lexer            tokens
  → parser           AST
  → resolver         names bound to declarations, modules loaded
  → type checker     typed AST
  → ownership check  moves, borrows, definite initialization
  → lowering         AIR: explicit control flow, drops and drop flags
  → codegen          LLVM IR (textual .ll)
  → clang            optimization, machine code, linking with the runtime
```

Generating **textual LLVM IR** instead of linking against the LLVM
libraries keeps the compiler free of C bindings and easy to self-host. The
generated code is optimized by exactly the same LLVM passes either way.

## Runtime

A small C file (`runtime/runtime.c`) provides what LLVM IR cannot express
portably: memory allocation, process entry, panics and system calls. It is
linked into every program. It will shrink over time as the standard library
moves to Astra.

## Bootstrap subset

Features stage 0 must support, in implementation order:

1. Lexer with automatic statement termination
2. Functions, `let`/`var`, integers, `bool`, arithmetic, `if`, `for`
3. `str`, `print`/`println`, string interpolation
4. Structs and methods
5. Enums with data, `match`, `if let`
6. `List<T>`, `Map<K, V>`, `Option<T>`, `Result<T, E>`, `Box<T>`, `?`
7. Moves and drops (the full borrow checker can come later in stage 1)
8. References `&T` / `&mut T`
9. Modules and `import`
10. `extern fn` and the `std.fs`, `std.os`, `std.io` subset

Stage 0 checks types but **may skip the borrow checker**: programs it
accepts are still valid Astra, because stage 1 enforces all rules once it
compiles itself.

## Milestones

| # | Milestone | Done when |
|---|-----------|-----------|
| M0 | Specification draft | `docs/spec` reviewed |
| M1 | Hello world | stage 0 compiles and runs `examples/hello.astra` |
| M2 | Bootstrap subset | stage 0 compiles all programs in `examples/` |
| M3 | Self-hosted front end | stage 1 lexes and parses its own source |
| M4 | Self-hosted | stage 1 compiles itself and stage2 == stage3 |
| M5 | Full language | borrow checker, diagnostics, generics |
