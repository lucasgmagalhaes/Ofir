# 0023. Closures are written as arrow functions

- Status: Accepted
- Date: 2026-11-01

## Context

Closures were specified with the keyword of named functions:
`fn(x) => x * 2`, `move fn() { ... }`. Callbacks are the most common
closures (`map`, `filter`, `sortBy`, `spawn`), and the keyword and the
parentheses add noise to short ones. JavaScript and TypeScript write them
as arrow functions, `x => x * 2`, a form most programmers read at once.

## Decision

A closure is an arrow function (spec 10.1):

```ofir
xs.map(x => x * 2)
xs.sortBy((a, b) => a.totalCmp(b))
test(arg => println(arg))
let add = (a: int, b: int) int => a + b
thread.spawn(move () => {
    send(compute())
})
```

- One parameter may go without parentheses: `x => ...`; none or several
  need them: `() => ...`, `(a, b) => ...`.
- Parameter types and the result type may be written, as in a function
  signature: `(a: int, b: int) int => a + b`; they are inferred from the
  expected function type otherwise.
- The body is an expression or a block: `x => { ... }`, where `return`
  returns from the closure.
- `move` before the parameters makes an owned closure: `move () => ...`.
- Function types keep their spelling: `fn(int) str`, `move fn()`.

`=>` also separates a `match` arm from its pattern. A closure in a `match`
guard must therefore be in parentheses, as struct literals must be in the
conditions of `if` (spec A.6).

## Consequences

- Short callbacks lose the `fn` keyword and, for one parameter, the
  parentheses.
- The parser recognizes a closure by looking ahead: an identifier followed
  by `=>`, or a parenthesized list whose `)` is followed by `=>` or by a
  type and `=>`. The lookahead is bounded by the parentheses, as for
  generic arguments (spec 8.5).
- `fn` now only starts a named function or a function type.
- Closures are not implemented yet (roadmap 5.7); no code changes.
