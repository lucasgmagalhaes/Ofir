# 0005. Integer arithmetic is checked in every build profile

- Status: Accepted
- Date: 2026-10-30

## Context

Silent integer overflow is a common source of security bugs. C makes signed
overflow undefined behavior; Rust checks only in debug builds.

## Decision

Integer overflow, division by zero, invalid shifts and lossy `as`
conversions panic in all build profiles. Wrapping and saturating arithmetic
is available through explicit methods.

## Consequences

- Programs are robust by default: overflow is always detected.
- Cost: one well-predicted branch per operation; LLVM removes checks it can
  prove unnecessary (e.g. loop counters bounded by a length). Measured costs
  in other languages are typically 1–5%.
- Hot code that needs wrapping semantics (hashing, cryptography) must use the
  explicit methods.
