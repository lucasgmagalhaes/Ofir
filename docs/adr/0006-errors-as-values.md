# 0006. Errors as values, panics abort

- Status: Accepted
- Date: 2026-10-30

## Context

Go's `(value, err)` pairs are verbose and easy to ignore; exceptions hide
control flow and require unwinding support.

## Decision

Recoverable errors use `Result<T, E>` with the `?` operator; ignoring a
`Result` is a compile error. Bugs cause panics, which print a message and
abort the process without unwinding.

## Consequences

- Error paths are visible in signatures and at call sites, with little noise.
- No unwinding tables: smaller binaries and simpler code generation.
- Destructors do not run on panic; resources are reclaimed by the OS.
- Panics cannot be caught; isolation requires separate processes (as done by
  the test runner).
