# 15. Compile-time code **[planned]**

Ofir has **no macros**. Code that must adapt to the shape of a type
(serialization, debug printing, comparisons, builders) is written as
ordinary generic code that inspects types **at compile time**. Like other
generic code, it is type-checked once, at its definition, and expanded for
each instance (ADR 0035). The implementation is planned for stage 1
(roadmap 6.7a).

Goals:

- the code that runs is ordinary, type-checked Ofir, readable and
  debuggable — no token-level transformations;
- no runtime reflection: everything is resolved during compilation and
  costs nothing at run time;
- libraries such as JSON encoders need no compiler support.

## 15.1 `comptime` blocks and constants

`comptime` marks code that the compiler executes during compilation. It may
use only constants, type information and pure functions (no I/O, no
allocation that escapes):

```ofir
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

These values exist only at compile time. `meta.fieldsOf<T>()` needs `T` to
be a struct, or narrowed to one by `comptime if meta.isStruct<T>()` (15.3);
`meta.variantsOf<T>()` likewise needs an enum (E0364).

## 15.3 `comptime for` and `comptime if`

`comptime for` is unrolled by the compiler. It iterates over `keyof T`
(16.6), `meta.fieldsOf<T>()` or `meta.variantsOf<T>()`. Its body is
type-checked once, with the loop variable standing for an implicit key
parameter `K: keyof T` (16.2), and expanded once per field of each instance,
in declaration order, with `K` bound to that field. `break` and `continue`
are not allowed in it.

`comptime if` keeps only the branch whose condition is true for the
instance. The condition is built from `true`, `false`, constants, the
`meta` predicates and `field.index`, with `!`, `&&` and `||` (E0363
otherwise). When it does not depend on a type parameter, only the taken
branch is type-checked. Otherwise both branches are, and in the `then`
branch the predicates **narrow** the type parameters they name:

| Predicate | Adds to `T` |
|-----------|-------------|
| `meta.implements<T, I>()` | the bound `I` |
| `meta.isInteger<T>()` | `Copy`, `Eq`, `Ord`, `Hash`, `Clone`, `Debug`, `Display` |
| `meta.isFloat<T>()` | `Copy`, `Clone`, `Debug`, `Display` |
| `meta.isStruct<T>()`, `meta.isEnum<T>()` | the use of `fieldsOf`, `variantsOf` |

Narrowing passes through `&&`; under `A || B` a parameter gets only what
both `A` and `B` give it, and `!` gives nothing.

```ofir
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

- `field.name` is the field's name (`str`), `field.index` its position
  (`int`) and `field.attribute(key)` its `@tag` value (`Option<str>`).
- `field.get(value)` is `value[K]`: its type is `T[K]`, the field's type in
  each instance. `toJson(field.get(value), out)` calls the instance of
  `toJson` for that type.
- `comptime panic(...)` has type `never`. Its message is a string literal
  whose interpolations are compile-time `str` values. Expanding an instance
  that keeps it is a compile error at the call that created the instance,
  with the given message (E0814).

## 15.4 Attributes as metadata

`@tag("key", "value")` attaches a string to a field, variant or type. Tags
have no effect on compilation; compile-time code reads them through
`field.attribute("key")` (an `Option<str>`). Other attributes keep the
meanings of chapter 12.1.

## 15.5 Limits

- The value of a `comptime` expression is a number, `bool`, `str`, array,
  tuple, struct or enum of those (E0818); what its functions allocate does
  not escape.
- Compile-time execution has a step budget (default 10 million operations,
  configurable with `@comptimeBudget(n)` on the enclosing declaration), so
  a buggy loop is a compile error instead of a hang. A panic, an
  arithmetic or bounds error, or running out of budget is E0816.
- `comptime` code is deterministic: it cannot read files, the clock, the
  environment or random numbers, nor call `extern` functions (E0815).
  Builds stay reproducible (required for the self-hosting check).
- A `comptime fn` called from code that runs at run time is E0817.
