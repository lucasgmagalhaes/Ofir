# 6. Error handling

Astra separates two kinds of failure:

- **Recoverable errors** (file not found, invalid input) are values of type
  `Result<T, E>` and must be handled.
- **Bugs** (index out of bounds, overflow, failed assertion) cause a
  **panic**, which stops the program.

There are no exceptions.

## 6.1 `Result` and `Option`

Both are built-in generic enums, behaving as if declared as:

```astra
enum Result<T, E> {
    Ok(value: T)
    Err(error: E)
}

enum Option<T> {
    Some(value: T)
    None
}
```

`Ok`, `Err`, `Some` and `None` are in scope everywhere without a prefix,
in expressions and in patterns (`Some(x)`, `None`). They are declared in
`std/core/option.at` and `result.at`, marked `@lang("option")` and
`@lang("result")`.

```astra
fn parsePort(s: str) Result<u16, str> {
    let n = parseInt(s)?
    if n < 1 || n > 65535 {
        return Err("port out of range: {n}")
    }
    return Ok(n as u16)
}
```

## 6.2 The `?` operator

`expr?` unwraps a `Result` or `Option`:

- on `Ok(v)` / `Some(v)`, the expression evaluates to `v`;
- on `Err(e)` / `None`, the enclosing function returns immediately with
  `Err(e)` / `None`.

The function's return type must be compatible: `?` on a `Result<T, E>`
requires the function to return `Result<U, E>`, and `?` on an `Option<T>`
requires it to return `Option<U>`.

If the error types differ, `?` widens the error when `F` is a union type
that contains `E` (chapter 17.3), and otherwise converts it when the
function's error type `F` satisfies `From<E>` (`F.from(e)` is called). Otherwise the
conversion is explicit with `.mapErr(...)`. Stage 0 requires both error
types to be equal.

## 6.3 Unused results

Ignoring a value of type `Result` is a compile error:

```astra
writeFile(path, data)        // error: unused Result
let _ = writeFile(path, data) // explicitly ignored
```

## 6.4 Helper methods

| Method | Behavior |
|--------|----------|
| `r.unwrap()` | the value, or panic with the error |
| `r.expect("msg")` | the value, or panic with `msg` |
| `r.unwrapOr(default)` | the value, or `default` |
| `r.isOk()` / `o.isSome()` | `bool` |
| `r.mapErr(f)` | convert the error (stage 1: needs closures) |

## 6.5 Panics

A panic prints a message and the source location, then terminates the
process with exit status 101. Destructors are **not** run on panic.

```
panic: index out of bounds: the length is 3 but the index is 5
  at src/main.at:12:17
```

Panics are raised by:

- out-of-bounds indexing;
- integer overflow, division by zero, invalid shifts;
- failed `as` conversions;
- `unwrap`/`expect` on `None` or `Err`;
- `assert(cond)` and `panic("message")`;
- stack overflow (detected by the runtime).

Panics cannot be caught. Recovering from panics is intentionally not
supported.
