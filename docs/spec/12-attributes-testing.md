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

Unknown attributes are compile errors.

## 12.2 Tests

Tests live next to the code they test. A test is a function marked `@test`
with no parameters, returning `()` or `Result<(), E>`:

```astra
fn add(a: int, b: int) int {
    return a + b
}

@test
fn add_works() {
    assert(add(2, 2) == 4)
    assert_eq(add(-1, 1), 0)
}
```

- `astra test` compiles the project with all `@test` functions and runs
  each one in a separate process, so a panic fails only that test.
- Tests are excluded from normal builds.
- `assert_eq(a, b)` prints both values (using `Debug`) when they differ.

## 12.3 Benchmarks

```astra
@bench
fn bench_sort(b: &mut Bencher) {
    var data = random_list(10_000)
    b.run(fn() => data.sort())
}
```

`astra bench` runs benchmarks with optimizations enabled and reports time
per iteration.

## 12.4 Conditional compilation

```astra
@cfg(os == "linux")
fn page_size() uint { return 4096 }

@cfg(debug)
fn trace(msg: &str) { eprintln(msg) }
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
pub fn max<T: Ord>(a: T, b: T) T { ... }
```
