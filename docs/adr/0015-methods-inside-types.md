# 0015. Methods are declared inside their type

- Status: Accepted (supersedes the method syntax of 0008)
- Date: 2026-10-30

## Context

The first drafts used Go's receiver syntax, `fn (p: Point) length() float`,
with methods written anywhere in the type's module. The project owner
prefers declaring methods inside the struct, as in Swift, Kotlin, C# or
Java, so that a type and its behavior are read in one place.

## Decision

- Methods and associated functions are declared in the body of their
  struct or enum, after the fields or variants.
- A method takes `self` as its first parameter, with the parameter modes of
  ADR 0014: `self`, `mut self`, `move self`.
- A function in the body without `self` is an associated function, called
  as `Type.name()` (or `Self.name()` inside the type).
- Fields and methods are always accessed through `self`; there is no
  implicit `this`.
- Interfaces use the same method syntax.

## Consequences

- A type's full API is visible in its declaration; tools and readers never
  need to search the module for methods.
- `self` becomes a reserved keyword.
- Long types produce long bodies; splitting a large type's methods across
  files is not possible. This is accepted: a type that large should be
  split into smaller types.
- Coherence (interface satisfaction never depends on imports) is now
  structural rather than a separate rule.
