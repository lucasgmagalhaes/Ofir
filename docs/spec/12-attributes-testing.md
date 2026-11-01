# 12. Attributes, testing and documentation

## 12.1 Attributes

Attributes attach metadata to a declaration. They start with `@`:

```astra
@inline
fn square(x: int) int { return x * x }

@repr(C)
struct Header {
    magic: u32
    size: u32
}
```

| Attribute | Applies to | Meaning |
|-----------|-----------|---------|
| `@inline` / `@inline(never)` | functions | inlining hint |
| `@cold` | functions | rarely called (e.g. error paths) |
| `@repr(C)` | structs, enums | C-compatible layout |
| `@packed` | structs | no padding |
| `@align(N)` | structs | minimum alignment |
| `@link("name")` | `extern` blocks | link a native library |
| `@export("name")` | `extern "C"` functions | export a C symbol |
| `@deprecated("msg")` | any | warn on use |
| `@test` | functions | a test (12.2) |
| `@bench` | functions | a benchmark (12.3) |
| `@cfg(cond)` | any | conditional compilation (12.4) |
| `@allow(category)` | any | silence a warning category (12.6) |
| `@tag("key", "value")` | types, fields, variants | metadata for compile-time code (chapter 15.4) |
| `@lang("type")` | structs, `std` only | the fields and methods of a primitive type (ADR 0019); `@lang("box")` and `@lang("list")` mark `Box<T>` and `List<T>` |

Unknown attributes are compile errors.

## 12.2 Tests

Tests live next to the code they test. A test is a function marked `@test`
with no parameters, returning `()` or `Result<(), E>`:

```astra
fn add(a: int, b: int) int {
    return a + b
}

@test
fn addWorks() {
    assert(add(2, 2) == 4)
    assertEq(add(-1, 1), 0)
}
```

- `astra test` compiles the project with all `@test` functions and runs
  each one in a separate process, so a panic fails only that test.
- Tests are excluded from normal builds.
- `assertEq(a, b)` prints both values (using `Debug`) when they differ.

## 12.3 Benchmarks

```astra
@bench
fn benchSort(b: mut Bencher) {
    var data = randomList(10_000)
    b.run(fn() => data.sort())
}
```

`astra bench` runs benchmarks with optimizations enabled and reports time
per iteration.

## 12.4 Conditional compilation

```astra
@cfg(os == "linux")
fn pageSize() int { return 4096 }

@cfg(debug)
fn trace(msg: str) { eprintln(msg) }
```

Available conditions: `os` (`"linux"`, `"macos"`, `"windows"`), `arch`
(`"x86_64"`, `"aarch64"`), `debug`, `test`, and combinations with `&&`,
`||`, `!`.

## 12.5 Documentation comments

Comments starting with `///` document the next declaration; `//!` documents
the enclosing module. They are Markdown. Code blocks inside them are
compiled and run by `astra test` as doc tests.

```astra
/// Returns the larger of two values.
///
/// ```astra
/// assert(max(1, 2) == 2)
/// ```
pub fn max<T: Ord>(a: move T, b: move T) T { ... }
```

## 12.6 Warnings

Errors stop compilation; warnings do not. Astra keeps warnings few and
meaningful:

| Warning | Example |
|---------|---------|
| unused variable | `let x = 1` never read (prefix with `_` to silence) |
| unused import | `import std.fs` never used |
| unused private function, type or constant | |
| unreachable code | statements after `return` |
| unused `mut` / `var` | a `var` that is never reassigned or mutated |
| deprecated item | uses of `@deprecated` items |
| naming convention | `fn read_file()`, `struct token` (chapter 1.4) |

- `--deny-warnings` turns every warning into an error; CI uses it.
- `@allow(unused)` on an item silences a warning category for that item.
- Ignoring a `Result` is an **error**, not a warning (chapter 6.3).
