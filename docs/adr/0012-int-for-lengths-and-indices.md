# 0012. `int` for lengths and indices

- Status: Accepted
- Date: 2026-10-30

## Context

Using an unsigned type for sizes (as Rust's `usize` and C's `size_t` do)
forces conversions whenever indices meet ordinary arithmetic, and makes
`i - 1` on index 0 a panic or a wrap-around.

## Decision

`len()` returns `int` and indexing takes `int`, as in Go. A negative index is
out of range and panics like any other out-of-range index. `uint` remains for
bit manipulation, hashing and C interop.

## Consequences

- Loop counters, lengths and indices mix freely without casts.
- Collections are limited to 2^63 − 1 elements, which is not a practical
  limit on 64-bit targets.
- The bounds check is a single unsigned comparison, as before.
