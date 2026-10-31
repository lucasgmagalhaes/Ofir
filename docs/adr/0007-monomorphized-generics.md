# 0007. Generics implemented by monomorphization

- Status: Accepted
- Date: 2026-10-30

## Context

Generics can be compiled by monomorphization (C++, Rust: one copy per type,
maximal speed, larger binaries) or by dictionary passing / boxing (Go,
Java: smaller binaries, slower).

## Decision

Generic functions and types are monomorphized. Dynamic dispatch is available
explicitly through `dyn` when code size matters more than speed.

## Consequences

- Generic code is as fast as hand-specialized code and fully inlinable.
- Binary size and compile time grow with the number of instantiations.
- Bounds are checked at definition time, so errors are reported in generic
  code itself, not at each instantiation.
