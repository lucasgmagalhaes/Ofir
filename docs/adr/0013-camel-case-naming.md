# 0013. camelCase naming convention

- Status: Accepted
- Date: 2026-10-30

## Context

The first drafts used Rust's conventions (`snake_case` functions). The
project owner prefers `camelCase`, which is also familiar from Go, Java,
JavaScript, TypeScript, Kotlin and Swift.

## Decision

- `camelCase` for functions, methods, variables, parameters and fields;
- `PascalCase` for types, interfaces, enum variants and type parameters;
- `SCREAMING_CASE` for constants;
- lowercase `snake_case` for modules and file names;
- acronyms written as words (`IoError`, `parseUrl`).

The conventions are checked by a warning, not enforced by the grammar
(spec 1.4).

## Consequences

- The standard library uses camelCase throughout (`readToString`,
  `splitWhitespace`, `sizeOf`).
- Lowercase module and file names avoid problems on case-insensitive file
  systems.
- C names in `extern "C"` blocks keep their original spelling.
