# 0026. The manifest is `ofir.json`, JSON with comments

- Status: Accepted
- Date: 2026-11-02

## Context

A project's manifest was `ofir.toml` (TOML). It is read and edited by
hand, so it needs comments, and adding an entry should not require
editing the previous line. The formats considered:

- **TOML**: comments, but its `[sections]` are apart from the values, and
  it is little known outside Rust and Python.
- **YAML**: significant indentation and known pitfalls (`no` is `false`,
  versions become numbers).
- **JSON**: known everywhere, but without comments or trailing commas.
- **Ofir code** (`project.of`, as Zig's `build.zig`): reading metadata
  would run code, which complicates tools and the package manager.

## Decision

The manifest is **`ofir.json`**, in JSON with comments: `//` and `/* */`
comments and trailing commas are allowed, as in `tsconfig.json` and the
settings of VS Code (the format editors call JSONC). Keys are camelCase,
as Ofir names:

```jsonc
{
  "name": "my_app",
  "version": "0.1.0",
  "edition": "2027",
  "build": {
    "kind": "exe",            // "exe" or "lib"
    "root": "src/main.of",
  },
  "dependencies": {},
  "fmt": {
    "inlineBlocks": false,
  },
}
```

The nearest directory holding an `ofir.json` is the project root (spec
7.1). The full format is in docs/design/toolchain.md.

## Consequences

- Editors highlight and check the manifest with their JSON support.
- A strict JSON parser does not read it; Ofir's own reader (and std's,
  when there is one) accepts the comments and trailing commas.
- The compilers only look for the file so far: the format matters from
  `ofir build` without arguments and the package manager on (roadmap 6.5).
