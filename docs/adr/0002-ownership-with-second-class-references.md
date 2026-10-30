# 0002. Ownership with second-class references

- Status: Accepted
- Date: 2026-10-30

## Context

Astra has no garbage collector. Memory safety without a GC requires either
reference counting (runtime cost), manual management (unsafe), or ownership
with borrow checking (Rust). Rust's model is powerful but its lifetime
annotations are the main source of its learning curve.

## Decision

Astra uses ownership and borrowing with **second-class references**:
references may be parameters, locals and temporaries, but cannot be stored
in data structures or returned, except when a function with exactly one
reference parameter returns a borrow of it, and through `ref struct` types
(spec chapter 5).

## Consequences

- No lifetime annotations, ever. Borrow checking is local to a function.
- Memory is released deterministically with no runtime cost beyond
  occasional drop flags.
- `&mut` never aliases, so the compiler can emit `noalias` for LLVM.
- Some designs common in Rust (structs holding references, long-lived
  iterators over borrowed data stored in fields) are not expressible; they
  are replaced by indices, owned values, `ref struct` or `Rc<T>`.
- The rules may be relaxed later (e.g. explicit lifetimes as an advanced
  feature) without breaking existing code; the reverse would not be possible.
