# 0003. Self-hosted compiler bootstrapped from a C compiler

- Status: Accepted
- Date: 2026-10-30

## Context

The final Astra compiler is written in Astra. It must first be compiled by
a compiler written in another language (stage 0). Candidates were Go, Rust
and C.

## Decision

Stage 0 is written in **C11**, implements only the subset of Astra needed by
the self-hosted compiler, and is frozen once the self-hosted compiler can
build itself.

## Consequences

- Building Astra from scratch needs only a C compiler and clang.
- C lacks sum types and safe memory management; stage 0 compensates by using
  arenas for all allocations and keeping its scope minimal.
- Since stage 0 is temporary, its code quality matters less than its
  correctness on the bootstrap subset.
- Long term, stage 0 may be replaced by a checked-in seed of generated LLVM IR.
