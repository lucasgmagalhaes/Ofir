# 12. Attributes, testing and documentation

## 12.1 Attributes

Attributes attach metadata to a declaration. They are written with `@`
before the declaration, `@name` or `@name(arguments)`; there is no
bracket form such as `#[test]` or `[test]`:

```ofir
@inline
fn square(x: int) int {
    return x * x
}

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
| `@align(N)` | structs | minimum alignment, `N` a power of two up to 4096 |
| `@link("name")` | `extern` blocks | link a native library |
| `@export("name")` | `extern "C"` functions | export a C symbol |
| `@deprecated("msg")` | any | warn on use |
| `@test` | functions | a test (12.2) |
| `@bench` | functions | a benchmark (12.3) |
| `@cfg(cond)` | any | conditional compilation (12.4) |
| `@allow(category)` | any | silence a warning category (12.6) |
| `@tag("key", "value")` | types, fields, variants | metadata for compile-time code (chapter 15.4) |
| `@lang("type")` | structs, `std` only | the fields and methods of a primitive type (ADR 0019); `@lang("box")`, `"list"`, `"option"` and `"result"` mark the std types the compiler knows |

Attributes are checked where they are declared:

- an unknown attribute is an error (E0807);
- an attribute on a declaration the table does not list for it is an
  error (E0808): `@inline` on a struct, `@packed` on a field;
- arguments that do not match the table are an error (E0809): `@cold(1)`,
  `@inline(always)`, `@align("8")`, `@align(8192)`;
- an attribute written twice on one declaration is an error (E0810).

"Functions" are top-level functions and methods; the functions declared
in an `extern` block have no body and take only `@deprecated`, `@cfg`
and `@allow`. A test file's declarations follow the same rules.

`@inline` asks for the function to be inlined at every call and
`@inline(never)` forbids it; `@cold` marks a function as unlikely to be
called, so the optimizer moves its calls out of the hot path. They are
hints with no effect on the meaning of the program.

`@deprecated` marks a function, method, type, constant or global that
should no longer be used: each use of its name outside a deprecated
declaration is a warning (W0006), "`name` is deprecated", followed by the
message if one is given:

```ofir
@deprecated("use `parseInt`")
fn atoi(s: str) int { ... }

let n = atoi("12")   // warning[W0006]: `atoi` is deprecated: use `parseInt`
```

`@allow(category)` silences the warnings of one category inside the
declaration it marks (12.6). The categories are `unused` (W0001, W0002,
W0003), `unreachable` (W0004), `deprecated` (W0006) and `naming` (W0007);
another identifier is an error (E0809).

Stage 0 and stage 1 implement `@inline`, `@cold`, `@test` and `@lang`;
stage 1 also implements `@deprecated`, `@allow`, `@cfg`, `@repr(C)`,
`@packed`, `@align`, `@export` and `@link`. `@bench` and `@tag` are checked,
then reported as not supported yet (E0901).

## 12.2 Tests

Tests live next to the code they test, in a test file: the tests of
`calc.of` are in `calc_test.of`, and those of a directory's `index.of` in
`index_test.of`. A test file belongs to the module it tests, so tests may
use its private declarations; only `ofir test` loads it. A test is a
function marked `@test` with no parameters, returning `()` or
`Result<(), E>`:

```ofir
// calc.of
fn add(a: int, b: int) int {
    return a + b
}

// calc_test.of
@test
fn addWorks() {
    assert(add(2, 2) == 4)
    assertEq(add(-1, 1), 0)
}
```

- `ofir test` compiles the project with all `@test` functions and runs
  each one in a separate process, so a panic fails only that test.
- Tests are excluded from normal builds: test files are not loaded, and a
  `@test` function written in an ordinary file (allowed, but not the
  convention) is not even checked, except for its signature (E0205).

Stage 0: `ofir test <file.of>` (or `<file>_test.of`) runs the tests of
every module of the program except the std modules, plus those of the root
file, which may be a std module (`ofir test std/core/split.of` runs
`std/core/split_test.of`); `make test-std` runs the
tests of std. A test returns `()`; `Result` tests and `assertEq` await
interfaces (roadmap 5.1). Doc tests are planned.
- `assertEq(a, b)` prints both values (using `Debug`) when they differ.

## 12.3 Benchmarks

```ofir
@bench
fn benchSort(b: mut Bencher) {
    var data = randomList(10_000)
    b.run(() => data.sort())
}
```

`ofir bench` runs benchmarks with optimizations enabled and reports time
per iteration.

## 12.4 Conditional compilation

```ofir
@cfg(os == "linux")
fn pageSize() int {
    return 4096
}

@cfg(debug)
fn trace(msg: str) {
    eprintln(msg)
}
```

Available conditions: `os` (`"linux"`, `"macos"`, `"windows"`), `arch`
(`"x86_64"`, `"aarch64"`), `debug`, `test`, and combinations with `&&`,
`||`, `!`.

`os` and `arch` are compared with `==` or `!=` to one of the values
listed; `debug` holds when the program is built without `--release`, and
`test` under `ofir test`. Any other condition, or a value that is not
listed, is an error (E0809).

A declaration whose condition is false is removed right after parsing,
before names are resolved: it declares nothing and is not checked, so
two declarations of one name may exist under conditions that exclude each
other. This holds for top-level declarations, imports, fields, methods and
the functions of an `extern` block.

## 12.5 Documentation comments

A `/** ... */` comment documents the next declaration; one whose text
starts with `@module` documents the file (ADR 0020). The text is Markdown:
a summary sentence, details, then tags in the style of JSDoc. Code blocks
inside them are compiled and run by `ofir test` as doc tests.

```ofir
/**
 * The larger of two values.
 *
 * @typeParam T the type of the values
 * @param a the first value
 * @param b the second value
 * @returns `a` if it is greater than `b`, else `b`
 * @example
 * ```ofir
 * assert(max(1, 2) == 2)
 * ```
 */
pub fn max<T: Ord>(a: move T, b: move T) T { ... }
```

| Tag | Meaning |
|-----|---------|
| `@module` | documents the file's module |
| `@typeParam T ...` | a type parameter |
| `@param name ...` | a parameter, in order, except `self` |
| `@returns ...` | the result of a function that returns a value |
| `@panics ...` | when the function panics |
| `@safety ...` | what the caller of an `unsafe fn` must guarantee |
| `@example` | an example in a fenced `ofir` block |
| `@see target` | a related declaration or document |
| `@deprecated reason` | the declaration should no longer be used |

Every public declaration of `std` is documented, and its tags match its
signature (`tools/check_docs.py`).

## 12.6 Warnings

Errors stop compilation; warnings do not. Ofir keeps warnings few and
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
- Unused variables (W0001) exclude parameters and names starting with
  `_`; `for _ in 0..n` repeats without a variable. Stage 0 reports unused
  variables, unused imports (W0002), unreachable code (W0004) and
  unreachable `match` arms; the other warnings come with stage 1.
- `@allow(unused)` on a declaration silences a warning category inside
  it (12.1).
- Ignoring a `Result` is an **error**, not a warning (chapter 6.3).
