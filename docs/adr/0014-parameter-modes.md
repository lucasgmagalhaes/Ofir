# 0014. Parameters are borrowed by default

- Status: Accepted (refines 0002)
- Date: 2026-10-30

## Context

With explicit references, almost every signature and call site carried `&`
or `&mut` (`fn total(xs: &List<int>)`, `total(&nums)`). The project owner
wanted users not to think about references in everyday code. Making every
struct a heap-allocated reference type (as Java, C# or Swift classes) was
rejected: it requires a garbage collector or reference counting, loses
`noalias` and reintroduces aliasing bugs.

## Decision

Parameters and receivers have one of three **modes** (spec 5.3):

| Declaration | Meaning |
|-------------|---------|
| `x: T` | borrow (default): read-only, the caller keeps ownership |
| `x: mut T` | mutable borrow: modifies the caller's value |
| `x: move T` | takes ownership |

Call sites never mark the mode: `reset(nums)`, not `reset(mut nums)`.
Loops use `for mut x in xs` for mutable element access. Explicit references
(`&T`, `&mut T`) remain, second-class as in ADR 0002, for local borrows,
returned borrows and `ref struct`.

## Consequences

- Everyday code has no `&`, and no garbage collector or reference counting
  is needed; performance and `noalias` are unchanged.
- The compiler chooses how to pass borrowed arguments (registers or
  pointer), which it can do because the callee cannot modify them.
- Mutation is visible in the signature but **not at the call site**; readers
  rely on method and function names (and editor hints) to see which
  arguments change. This was an explicit trade-off for less noise.
- Functions that store an argument must declare it `move`; the compiler's
  error message suggests this when it is missing.
