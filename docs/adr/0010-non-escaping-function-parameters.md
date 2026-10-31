# 0010. Non-escaping function values by default

- Status: Accepted
- Date: 2026-10-30

## Context

Closures that capture references must not outlive the referenced values.
With a single function type, a callee cannot know whether a closure it
receives captured references, so it could store it and break memory safety.
Rust solves this with lifetimes, which Astra avoids (ADR 0002).

## Decision

There are two function types (spec 10.2):

- `fn(A) R` — borrowed, second-class like a reference, Copy. Any function or
  closure converts to it. Function-typed parameters use it by default.
- `move fn(A) R` — owned, first-class, move-only. Only named functions and
  `move` closures (which capture by value, immutably) convert to it.

## Consequences

- Callbacks are cheap (no allocation) and safe without lifetimes; this is
  the same choice as Swift's non-escaping closures.
- Storing a callback is visible in the signature (`move fn`).
- `move` closures cannot mutate their captured state; mutable state goes in
  a struct with methods. This keeps function values free of hidden aliasing.
