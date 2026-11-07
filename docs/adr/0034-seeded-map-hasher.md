# 0034. Seeded `Map` hashing: SipHash keys and a per-map seed

- Status: Accepted
- Date: 2026-11-07

## Context

ADR 0011 made the default `Map` hasher deterministic, so compiler output
never depends on hash values, and left maps filled from untrusted input
exposed to hash flooding: an attacker who knows the hash function can send
keys that all land in one probe sequence, so each lookup takes linear time.
The default string hash was FNV-1a, whose collisions are easy to craft.

`hashOf(key)` is an intrinsic lowered by both compilers into calls to
`hashStr`, `hashU64` and `hashCombine` (`std.core.hash`), with derived
glue for tuples and ADTs. Threading a seed through the intrinsic would
change stage 0, stage 1 and the glue.

## Decision

- `hashStr` is SipHash-1-3 with a fixed key. It is still deterministic,
  but full 64-bit collisions cannot be found faster than by brute force.
  Integers and `bool` keep `hashU64`, a bijection, so they never fully
  collide; tuples and ADTs keep `hashCombine`.
- A map can carry a seed: `Map.seeded()` takes a random one from the
  operating system (`ofir_random_u64`: `getentropy`, `rand_s` on Windows),
  `Map.withSeed(seed)` a given one. A seeded map stores
  `hashSeeded(hashOf(key), seed)`, SipHash-1-3 of the one word keyed by the
  seed, so the slots of the keys cannot be predicted.
- `Map.new()` keeps seed 0: the deterministic hash of ADR 0011.

## Consequences

- No compiler change: the seed lives in `std/core/map.of`, and the
  intrinsic and its glue are unchanged.
- Flooding a seeded map needs keys whose `hashOf` values fully collide,
  which SipHash makes impractical for strings; iteration order is still the
  insertion order, so a seeded map's output stays deterministic.
- Hashing a string costs a few more instructions than FNV-1a (under 1% on
  the benchmarks); a seeded map pays one more SipHash per lookup.
- A `Hasher` interface (spec 9.9) can later replace the fixed functions
  without changing `seeded` or `withSeed`.
