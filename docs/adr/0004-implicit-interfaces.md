# 0004. Implicit interfaces with static and dynamic dispatch

- Status: Accepted
- Date: 2026-10-30

## Context

Polymorphism options: Go's implicit interfaces (simple, always dynamic
dispatch), Rust's explicit traits (zero cost, more ceremony), or classes and
inheritance (rejected: complexity, fragile hierarchies).

## Decision

Interfaces are satisfied implicitly, as in Go. They can be used as generic
bounds (static dispatch, monomorphized) or behind `dyn` (dynamic dispatch via
vtables). A type may optionally assert the interfaces it satisfies
(`struct Circle: Shape`). Methods can only be defined in the type's module.

## Consequences

- Little ceremony, as in Go; zero-cost abstraction when used as bounds.
- Restricting methods to the defining module keeps satisfaction coherent
  (it never depends on imports) and avoids Rust's orphan-rule complexity.
- A type cannot be adapted to a foreign interface without a wrapper struct.
- Accidental satisfaction is possible but harmless; assertions document
  intent where it matters.
