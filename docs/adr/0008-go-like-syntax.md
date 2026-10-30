# 0008. Go-like syntax without semicolons

- Status: Accepted; the newline rule is superseded by [0009](0009-parser-driven-newlines.md) and the method syntax by [0015](0015-methods-inside-types.md)
- Date: 2026-10-30

## Context

The language should feel as light as Go while supporting richer features
(enums, pattern matching, generics, ownership).

## Decision

- Statements are terminated by newlines using Go's insertion rule.
- `fn`, `let`/`var`, return types written right after the parameters
  (`fn name() str`) as in Go, `{}` blocks with mandatory braces
  and no parentheses around conditions.
- One loop keyword (`for`); `if` and `match` are expressions.
- Methods are declared with a receiver before the name, as in Go.
- Generic arguments use `<>`, disambiguated as described in spec 8.5.

## Consequences

- Familiar to Go, Rust and Swift programmers.
- The newline rule constrains formatting (`{` and `else` placement), which is
  also enforced by the formatter.
- `<>` generics need a disambiguation rule in expressions.
