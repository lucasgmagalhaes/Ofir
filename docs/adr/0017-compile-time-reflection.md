# 0017. Compile-time reflection instead of macros

- Status: Accepted
- Date: 2026-10-31

## Context

Without any metaprogramming, serialization, structured logging or custom
derivations need hand-written code per type. Options considered: textual or
token macros (C, Rust), runtime reflection (Go, Java), and compile-time
evaluation with type introspection (Zig's `comptime`).

## Decision

Ofir has **no macros and no runtime reflection**. It will provide
`comptime` evaluation, `std.meta` type information and `comptime for` /
`comptime if` (spec chapter 15), with `@tag` attributes as metadata.

## Consequences

- Generic libraries (JSON, debug printing, ORMs) are ordinary, type-checked
  Ofir code with zero runtime cost.
- No new syntax can be invented by users, which keeps code readable and
  tooling (formatter, language server) simple.
- Compile-time execution needs an interpreter inside the compiler, which is
  a significant stage 1 component; it is planned after M5.
- Compile-time code is sandboxed and deterministic, preserving reproducible
  builds.
