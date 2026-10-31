# 15. Compile-time code **[planned]**

Astra has **no macros**. Code that must adapt to the shape of a type
(serialization, debug printing, comparisons, builders) is written as
ordinary generic code that inspects types **at compile time**. The design is
fixed now; the implementation is planned for stage 1 (roadmap Phase 5).

Goals:

- the code that runs is ordinary, type-checked Astra, readable and
  debuggable — no token-level transformations;
- no runtime reflection: everything is resolved during compilation and
  costs nothing at run time;
- libraries such as JSON encoders need no compiler support.

## 15.1 `comptime` blocks and constants

`comptime` marks code that the compiler executes during compilation. It may
use only constants, type information and pure functions (no I/O, no
allocation that escapes):

```astra
const TABLE: [u32; 256] = comptime crc32Table()

comptime fn crc32Table() [u32; 256] { ... }    // usable only at compile time
```

A `comptime fn` can run only at compile time; an ordinary function can be
called from `comptime` code if its body only does what compile-time code
allows.

## 15.2 Type information

The module `std.meta` exposes compile-time descriptions of types:

| Function | Returns |
|----------|---------|
| `meta.fieldsOf<T>()` | the fields of struct `T`: `name`, `type`, `index`, attributes |
| `meta.variantsOf<T>()` | the variants of enum `T` and their fields |
| `meta.nameOf<T>()` | the type's name, as a constant `str` |
| `meta.isStruct<T>()`, `isEnum`, `isInteger`, ... | type classification |
| `meta.implements<T, I>()` | whether `T` satisfies interface `I` |

These values exist only at compile time.

## 15.3 `comptime for` and `comptime if`

`comptime for` is unrolled by the compiler: its body is type-checked once per
element, with that element's concrete types. `comptime if` keeps only the
branch whose condition is true, so the other branch may contain code that
would not type-check for this instantiation.

```astra
import std.meta

pub fn toJson<T>(value: T, out: mut str) {
    comptime if meta.isInteger<T>() || meta.isFloat<T>() {
        out.push("{value}")
    } else comptime if meta.isStruct<T>() {
        out.push("{")
        comptime for field in meta.fieldsOf<T>() {
            comptime if field.index > 0 {
                out.push(",")
            }
            out.push("\"{field.attribute("json") ?? field.name}\":")
            toJson(field.get(value), out)
        }
        out.push("}")
    } else {
        comptime panic("toJson: unsupported type {meta.nameOf<T>()}")
    }
}

struct User {
    @tag("json", "user_id")
    id: int
    name: str
}
```

- `field.get(value)` reads the field with its concrete type.
- `comptime panic(...)` produces a compile error at the instantiation that
  reaches it, with the given message.

## 15.4 Attributes as metadata

`@tag("key", "value")` attaches a string to a field, variant or type. Tags
have no effect on compilation; compile-time code reads them through
`field.attribute("key")` (an `Option<str>`). Other attributes keep the
meanings of chapter 12.1.

## 15.5 Limits

- Compile-time execution has a step budget (default 10 million operations,
  configurable with `@comptimeBudget(n)`), so a buggy loop is a compile
  error instead of a hang.
- `comptime` code is deterministic: it cannot read files, the clock, the
  environment or random numbers. Builds stay reproducible (required for the
  self-hosting check).
