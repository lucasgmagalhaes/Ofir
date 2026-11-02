# 0001. Generate textual LLVM IR and compile it with clang

- Status: Accepted
- Date: 2026-10-30

## Context

Ofir must produce code as fast as C. Options considered:

1. **Generate C** and compile with a C compiler: portable and simple, but C
   cannot express `noalias` guarantees precisely, has undefined behavior
   pitfalls (signed overflow) and limits control over the ABI and debug info.
2. **Link against the LLVM C API**: full control, but ties the compiler to a
   specific LLVM version, requires C bindings in the self-hosted compiler and
   makes building Ofir harder.
3. **Emit textual LLVM IR** and pass it to `clang`.
4. **Write our own backend**: years of work to approach LLVM's optimizer.

## Decision

The compiler emits textual LLVM IR (`.ll`) and invokes `clang` to optimize,
assemble and link it with the runtime.

## Consequences

- Generated code gets the same optimizations as C and Rust.
- The self-hosted compiler needs no FFI to LLVM; it only writes text files and
  runs a process.
- The only external dependency to build Ofir programs is clang.
- Writing and re-parsing text costs some compile time (typically a few
  percent of the time LLVM spends optimizing).
- The textual IR format changes between LLVM versions (for example, LLVM 21
  replaced the `nocapture` attribute with `captures(none)`). Mitigations:
  - a **supported range** of LLVM versions (initially 17 to the latest
    release), each tested in CI;
  - the generator uses only long-stable IR constructs and emits
    version-dependent syntax from a single table keyed by the detected
    `clang --version`;
  - opaque pointers only (LLVM ≥ 17 accepts nothing else).
