# 0011. Insertion-ordered `Map` with deterministic hashing

- Status: Accepted
- Date: 2026-10-30

## Context

The self-hosting check (stage 2 == stage 3) requires deterministic compiler
output. Maps with unspecified iteration order or randomly seeded hashing make
output depend on hash values and differ between runs.

## Decision

`Map<K, V>` iterates in insertion order and its default hasher has no random
seed (spec 2.6). The implementation is a dense entry list plus an
open-addressing index table.

## Consequences

- Programs, including the compiler, behave identically on every run; tests
  can print maps directly.
- Iteration is faster (contiguous entries); removal costs slightly more.
- Maps filled with untrusted keys are exposed to hash-flooding; a seeded
  map (`Map.seeded()`, ADR 0034) is for that use case.
