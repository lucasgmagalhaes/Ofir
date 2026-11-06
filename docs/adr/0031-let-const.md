# 0031. `let` is mutable, `const` is immutable; `var` is removed

- Status: Accepted
- Date: 2026-11-06

## Context

Ofir had three binding keywords: `let` (an immutable local, with a
pattern), `var` (a mutable local or global) and `const` (a module
constant). In TypeScript, which much of Ofir's surface follows, `let` is
the binding that changes and `const` the one that does not; Ofir read the
other way round. Three keywords for two ideas also made the grammar and
the docs longer.

## Decision

- `let` declares a **mutable** binding: a local (`let count = 0`,
  `let buffer: List<u8>`) or, at module level, a mutable global
  (`let HITS: i32 = 0`). It binds a name, as `var` did.
- `const` declares an **immutable** binding. In a function it is the old
  `let`: any value, a pattern, and `const pattern = value else { ... }`.
  At module level it is a constant, whose value must still be constant
  (spec 3.2).
- Pattern conditions use `const`: `if const Option.Some(x) = value`.
- `var` is no longer a keyword. The switch is direct: both compilers, the
  std, the tests and the docs move at once, with no period where both
  spellings are accepted.

`let mut` (Rust) was not taken: it spends two words on the common case
of a counter or a buffer. Keeping `var` next to `let`/`const` was not
taken either: three keywords for two ideas is what this removes.

## Consequences

- Every `let` of the old code becomes `const` and every `var` becomes
  `let`; only the keyword changes, so the meaning of a program is the same.
- Diagnostics that suggested `var` now suggest `let` (E0304, E0315).
- `var` can be used as a name.
- A mutable binding still binds a name only: destructuring into mutable
  bindings stays out, as before.
