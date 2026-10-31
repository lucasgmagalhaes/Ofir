# 0009. Parser-driven statement termination

- Status: Accepted
- Date: 2026-10-30

## Context

ADR 0008 adopted Go's rule: the lexer turns a newline into a terminator when
the line ends with certain tokens. Go can do this because its generics use
`[]`. With `<>` generics, `var buffer: List<u8>` ends with `>`, which the
token rule cannot distinguish from a comparison continued on the next line.

## Decision

The lexer emits a newline token at the end of every line (outside `()` and
`[]`). The parser treats it as a terminator only where the grammar allows
the current statement to end, and skips it everywhere else (spec 1.3).

## Consequences

- `<>` generics and newline termination work together.
- Lines may end with any binary operator to continue an expression; a line
  starting with an operator begins a new statement, as in Go.
- The parser must call `skip_newlines()` at every position that requires
  more tokens; this is mechanical and tested by `tests/parse`.
