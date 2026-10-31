# 0016. Defaults, named arguments, `let ... else` and `??`; no overloading

- Status: Accepted
- Date: 2026-10-31

## Context

A review of the specification found everyday patterns that were verbose:
configuration structs needing every field, functions with many optional
settings, nested `match` to unwrap values, and defaults for `Option`.

## Decision

- **Default field values** (`port: int = 8080`) and **tuple structs**
  (`struct UserId(int)`).
- **Default parameter values and named arguments** (`connect(host, timeout: 5)`).
- **No overloading**: one function per name in a scope.
- **`let pattern = expr else { diverge }`** for unwrapping without nesting.
- **`??`** for `Option` defaults. No `?.`: the `?` operator already
  short-circuits inside functions returning `Option`.

## Consequences

- Fewer constructor functions and builder types; optional settings live in
  the signature.
- Parameter names become part of the public API of `pub` functions.
- Without overloading, a call always resolves to one function by name,
  which keeps type checking simple and error messages precise.
- Defaults, `let ... else`, `??` and tuple structs are in the bootstrap
  subset; named arguments and parameter defaults come with stage 1.
