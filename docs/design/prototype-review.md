# Prototype review: stage 1 lexer

Roadmap task 0.8. Before writing stage 0, the lexer of the self-hosted
compiler was written in the bootstrap subset (~650 lines, not compiled) to
check that the language and the subset are sufficient. It has since been
split into [`compiler/syntax`](../../compiler/syntax) `token.at`, `cursor.at`,
`literals.at` and `lexer.at` to follow the size rules of the style guide.

## Verdict

The bootstrap subset is **sufficient** for the lexer. Two design flaws in the
specification were found and fixed; the other findings were small gaps in the
standard library and in the ownership rules.

## Findings

| # | Finding | Severity | Resolution |
|---|---------|:--------:|------------|
| F1 | Ignoring every newline inside `(...)` breaks closures passed as arguments (`spawn(move fn() { ... })`): their statements would merge | high | Newlines are ignored only when the innermost open bracket is `(` or `[` (spec 1.3); the lexer keeps a bracket stack |
| F2 | A `>>` token breaks nested generics: `List<List<int>>` | high | The lexer always emits single `>`; the parser joins adjacent ones (spec 1.6) |
| F3 | No way to copy a `&str` into an owned `str` | medium | `str.toOwned()` and `[T].toList()` added to `std` |
| F4 | Encoding `\u{...}` escapes needs to append a code point | medium | `str.pushChar(c: u32)` added; `pushByte`, which could create invalid UTF-8, removed |
| F5 | `lx.interpolations.push(lx.brackets.len())` needs field-precise and two-phase borrows | medium | Borrows are tracked per place; method arguments are evaluated before the `&mut` receiver borrow is active (spec 5.5) |
| F6 | Comparing token kinds needs `==` on fieldless enums, and `Option<u64>` must be copyable | low | `==` on fieldless enums is in the bootstrap subset; `Option`/`Result`/tuples are Copy when their contents are (spec 2.9) |
| F7 | `pair.0.1` lexes `0.1` as a float | low | The parser splits the float token in field position, as in Rust |
| F8 | No error code for an unescaped `}` inside a string | low | `E0006` registered |

## Patterns that worked well

- **State without borrows.** A struct cannot hold a `&str` in the bootstrap
  subset (no `ref struct`). The lexer keeps only positions and passes the
  source to each method as a parameter. Tokens store spans instead of
  slices, which is also what the rest of the compiler needs.
- **`match` on bytes and tuples of bytes** (`match (c, next) { (b'0', b'x') => ... }`)
  replaces most of the character-class code a C lexer needs.
- **`?` on `Option`** keeps overflow-checked digit accumulation short.
- **Enum shorthand** (`.Plus`, `.Some(x)`) keeps the punctuation table
  readable without imports.

## Open points for later prototypes

- The parser and type checker should get the same treatment before Phase 4,
  in particular to validate unbounded generics for `IndexVec<I, T>` and the
  absence of closures (visitors become explicit `match` functions).
- `compiler/base` (`Span`, `FileId`, `Interner`) and `compiler/diag`
  (`Diagnostics`) are assumed by the prototype and still need to be written.
