# 0021. Type operators: `keyof`, indexed access and mapped types

- Status: Accepted
- Date: 2026-11-01

## Context

Code often needs a type derived from another one: every field of a struct
made optional (a patch, a form, a configuration override), a subset of its
fields (a view sent over the network), a struct with one field per key
(counters, settings). Writing these by hand duplicates the field list and
lets the copies drift apart.

TypeScript answers with type operators: `keyof T`, indexed access `T[K]`,
mapped types `{ [K in keyof T]: ... }` and conditional types, all erased
before the program runs. Ofir's types are nominal, its unions are tagged
(ADR 0022) and it has no runtime reflection (ADR 0017), so the operators
must be adapted, not copied.

## Decision

Ofir adopts type operators that are resolved entirely during compilation
(spec chapter 16):

- `keyof T` is a **key type**: the names of the fields of struct `T` (or
  the indices of a tuple), written as key literals `.name`. Key types
  combine with `|`, `&` and `-` (union, intersection, difference).
- `T[K]` is the type of the field `K` of `T`; `value[key]` is that field,
  when `key` is a key known at compile time.
- A **mapped type** `{ [K in Keys]: F }` is a struct with one field per key,
  in field order; `?:` makes each field an `Option` defaulting to `.None`,
  and `if` filters the keys.
- A **conditional type** `if A is B { X } else { Y }` chooses a type, and
  `infer U` names a part of `A`.
- `std.core` defines `Partial`, `Required`, `Pick`, `Omit` and `Record`
  with them.

Key types have no values at run time: a parameter of a key type is erased,
and each call is instantiated for its key, as generics are (ADR 0007).

## Consequences

- Derived types follow their source automatically: adding a field to
  `Config` adds it to `Partial<Config>`.
- No runtime cost and no new runtime representation; layouts are those of
  ordinary structs.
- A mapped type instance is a nominal struct named by its alias and
  arguments (`Partial<Point>`); it is not interchangeable with a
  structurally equal hand-written struct.
- `keyof` respects visibility: outside its module, a struct's private
  fields are not keys.
- The implementation needs generic type aliases and type-level evaluation
  in the checker; it is planned for stage 1 (roadmap 5.16 to 5.19), with
  `comptime for` over keys after compile-time code (chapter 15).
