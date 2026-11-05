# 0028. Variants are always named through their enum

- Status: Accepted
- Date: 2026-11-04

## Context

Until now a variant could be written in three ways:

- qualified: `Token.Plus`, `Shape.Circle(r)`;
- the shorthand `.Plus`, `.Circle(r)`, when the expected type gives the
  enum (Swift's implicit member expressions);
- bare, for the four prelude variants: `Some(x)`, `None`, `Ok(v)`,
  `Err(e)`.

The shorthand and the bare names save characters but hide which enum a
value belongs to. The reader must find the expected type to know what
`.None` is, and the shorthand only works where the checker knows that
type: a `match` arm, a typed `let`, an argument. A change elsewhere, such
as removing a type annotation, can break an unrelated line (E0320 "cannot
tell which enum"). The four bare prelude names are a special case in both
compilers' name resolution, and they take four names from every scope.

## Decision

A variant is **always named through its enum**, in expressions and in
patterns: `Token.Plus`, `Option.Some(x)`, `Option.None`, `Result.Ok(v)`,
`Result.Err(e)`. The shorthand `.V` and the bare prelude variants are
errors (E0326). The error names the enum when the checker knows it, so
the fix is mechanical.

`Option` and `Result` stay in the prelude; only their variants lose the
bare form. A module that uses another module's enum imports the enum
(`import syntax.token.TokenKind`) and writes `TokenKind.Ident`.

## Consequences

- Every variant names its type where it is written; no line depends on an
  inferred expected type to resolve.
- Code is longer, mostly in `match` arms and `==` comparisons. The
  compiler and `std` grew about 150 lines, which were wrapped to the
  100-column limit.
- Name resolution loses the prelude variant special case in both stages.
- A future `use Enum.*` (importing an enum's variants into scope) could
  bring back short names explicitly, per module; it is not planned.
