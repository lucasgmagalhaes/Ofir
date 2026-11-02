# 0022. Union types as anonymous tagged enums

- Status: Accepted
- Date: 2026-11-01

## Context

Functions that can fail in more than one way need an error type that covers
every failure. Today that means a hand-written enum per module, with one
variant per wrapped error and a `From` conversion for each, so that `?` can
convert (spec 6.2). The enum adds nothing but names, and keeping it in step
with the code that fails is busywork.

TypeScript answers with union types: `A | B` holds an `A` or a `B`. Its
unions rely on every value carrying its type at run time and on structural
subtyping. Ofir has neither: there is no GC and no runtime type
information, and an owned value must know which `drop` to run. A union
value must therefore carry a tag, which makes it an enum whose variants
are named by their types.

## Decision

Ofir adds **union types** (spec chapter 17): `A | B` is an anonymous
tagged enum with one variant per member type.

- A union is normalized: nested unions are flattened, repeated members
  removed and members sorted in a canonical order, so `A | B`,
  `B | A` and `A | (B | A)` are the same type.
- A value of a member type, or of a union whose members are all members,
  **widens** to the union at the coercion points: assignment and `let` with
  a declared type, arguments, `return`, struct and variant fields, and the
  error of `?`. There is no subtyping elsewhere: `List<A>` is not a
  `List<A | B>`.
- A union is **narrowed** by type test patterns, `e: IoError`, in `match`
  (which must be exhaustive) and `if let`.
- Members cannot be bare type parameters, nor two types that become equal
  for some instantiation (`List<T> | List<int>`), so the members and the
  layout of a union never depend on the instantiation.
- The layout is that of an enum with a one-field variant per member; drop
  and clone come from the same glue as enums.

`|` already combines key types (ADR 0021); a `|` between key types is a key
type, between other types a union, and mixing them is an error.

## Consequences

- Error composition needs no boilerplate: `Result<T, IoError | ParseError>`
  and `?` widen on their own.
- No new runtime representation: unions are enums the compiler names.
- The `match` on a union reads like one on an enum, with the member types
  in place of variant names.
- Union types are not inferred: `if c { 1 } else { "a" }` is an error
  until the expected type is written (E0353). This keeps inference
  local and error messages precise.
- Unions of literal values (`"a" | "b"`) are not adopted; an enum without
  payloads does that job.
- The implementation needs the checker's coercion points and the pattern
  checker of stage 1; it is planned for phase 5 (roadmap 5.20 and 5.21).
