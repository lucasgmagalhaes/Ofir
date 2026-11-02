# 0020. Documentation comments in the style of JSDoc

- Status: Accepted
- Date: 2026-11-01

## Context

Declarations were documented with `///` lines and modules with `//!`
lines, in free Markdown. Nothing said what a comment must contain, so
parameters, results and panics were described unevenly or not at all.
JSDoc (and TSDoc) solve this with tags that tools can check and render,
and many programmers already know them.

## Decision

- `/** ... */` documents the next declaration: functions, methods,
  structs, enums and their variants, fields, constants. Inside it, the
  leading `*` of each line is not part of the text, which is Markdown.
  `//` and `/* ... */` are ordinary comments; `///` and `//!` are no
  longer special. `/***` and `/**/` are ordinary comments, as in JSDoc.
- A file's documentation is a `/** ... */` comment whose text starts with
  the tag `@module`.
- The text starts with a summary sentence, then details. Tags follow:

  | Tag | Meaning |
  |-----|---------|
  | `@module` | the comment documents the file's module |
  | `@typeParam T description` | a type parameter (TSDoc's name) |
  | `@param name description` | a parameter, in the order of the signature; not `self` |
  | `@returns description` | the result, for a function that returns a value |
  | `@panics description` | when the function panics (Ofir has no exceptions, so no `@throws`) |
  | `@safety description` | what the caller of an `unsafe fn` must guarantee |
  | `@example` | an example, as a fenced ```` ```ofir ```` block (a doc test, spec 12.5) |
  | `@see target` | a related declaration or document |
  | `@deprecated reason` | the declaration should no longer be used |

- In `std`, every public declaration has such a comment, and the tags
  match the signature: `@param` for each parameter, `@typeParam` for each
  type parameter, `@returns` exactly when there is a result, `@safety`
  exactly for `unsafe fn`, `@panics` when the body calls `panic`.
  `tools/check_docs.py` enforces this in `make test` until `ofir doc`
  (stage 1) checks and renders comments itself.

## Consequences

- Documentation has a predictable shape that editors, `ofir doc` and the
  checker understand; a missing or stale `@param` is a test failure.
- The stage 0 lexer produces one token per documentation comment, with
  the text normalized; the parser still drops it, as there is no
  documentation generator yet.
- Code that used `///` and `//!` (std, tests, the `compiler/` prototype)
  was converted.
- Lexing the documented prelude costs a little more: +1.6% instructions
  on check-10k.
