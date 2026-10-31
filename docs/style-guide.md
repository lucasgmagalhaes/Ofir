# Astra style guide

The style `astra fmt` produces and that all code in this repository follows.
When the formatter exists it is the authority; until then, this document is.

## Layout

- **Indentation:** 4 spaces, never tabs.
- **Line width:** 100 characters. Break long lines after an operator, a `,`
  or an opening bracket (spec 1.3), and indent the continuation by 4 spaces.
- **Braces:** the opening `{` stays on the line of its construct; `else`
  follows the closing `}` on the same line.
- **One statement per line.** Short bodies may stay on one line when they
  contain a single statement: `fn name(self) str { return "circle" }`.
- **Blank lines:** one between top-level items and between methods; none at
  the start or end of a block.
- **Trailing commas** in every multi-line list (arguments, literals,
  parameters); none in single-line lists.
- **Spaces:** around binary operators and `=`; after `,` and `:`; none inside
  brackets, before `(` in calls, or around `.` and `..`.

```astra
let total = items.len() + extra
let point = Point{x: 1.0, y: 2.0}
for i in 0..n {
    process(list[i], options)
}
```

## Files

Order of a file's contents:

1. `//!` module documentation;
2. imports, sorted alphabetically, standard library first, separated from
   project imports by a blank line;
3. public types, then their related private types;
4. public functions, then private functions;
5. `@test` functions, at the end.

## Types

- Order inside a type: fields (or variants), associated functions
  (constructors first, `new` before others), then methods, then `drop`.
- Prefer `fn new(...) T` as the main constructor name; other constructors
  are named after what they take: `fromStr`, `withCapacity`.
- Fields that have a sensible default declare it (`port: int = 8080`)
  instead of requiring it in every literal.
- One-field wrappers use tuple structs: `copy struct NodeId(int)`.

## Names

Spec 1.4 defines the conventions (`camelCase`, `PascalCase`,
`SCREAMING_CASE`, lowercase file names). In addition:

- Booleans read as questions: `isEmpty`, `hasErrors`, `canRetry`.
- Functions that may fail return `Result` and are named for what they do,
  not `tryX`; panicking variants say so (`unwrap`, `expect`).
- Conversions: `toX` allocates or copies (`toOwned`, `toList`), `asX`
  borrows (`asRef`), `intoX` consumes (`intoVec2`).
- Avoid abbreviations except well-known ones (`id`, `len`, `src`, `ctx`).

## Control flow

- Handle the failure case first and keep the main path unindented with
  `let ... else` or early `return`.
- Prefer `match` over chains of `if` on the same enum.
- `match` arms are one line when they fit; otherwise use a block.
- Use `??` for defaults instead of `match` on `Option`.

```astra
fn load(path: str) Result<Config, IoError> {
    let text = fs.readToString(path)?
    let .Some(first) = text.lines().first() else {
        return Ok(Config{})
    }
    return parseConfig(first)
}
```

## Comments

- `///` documents every `pub` item: one summary sentence, then details.
- Ordinary comments explain *why*, not *what*. Write full sentences.
- `// TODO(name): ...` for known follow-ups, with a roadmap task or issue
  number when one exists.
