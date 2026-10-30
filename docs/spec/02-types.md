# 2. Types

Astra is statically typed. Every expression has a type known at compile
time. Local variable types are inferred; function signatures and struct
fields are always annotated.

## 2.1 Primitive types

| Type | Description |
|------|-------------|
| `i8`, `i16`, `i32`, `i64` | signed integers |
| `u8`, `u16`, `u32`, `u64` | unsigned integers |
| `int` | alias for `i64` |
| `uint` | alias for `u64` (used for sizes and indices) |
| `f32`, `f64` | IEEE-754 floats |
| `float` | alias for `f64` |
| `bool` | `true` or `false` |

There are no implicit numeric conversions. Conversions are explicit with
`as`:

```astra
let a: i32 = 10
let b = a as i64
let c = 3.9 as int      // 3 (truncates toward zero)
```

`as` between integer types panics if the value does not fit the target
type. Float-to-integer `as` panics on NaN or out-of-range values.

### Arithmetic

Integer arithmetic is **checked**: overflow, division by zero and shifts
by more than the bit width panic. The cost is a single branch that is
almost never taken; LLVM's overflow intrinsics make it close to free.
Explicit wrapping arithmetic is available through methods
(`a.wrapping_add(b)`, `a.wrapping_mul(b)`, ...).

## 2.2 The unit type

`()` is the type with a single value, also written `()`. Functions without
a return type return `()`.

## 2.3 Strings

`str` is an owned, growable, UTF-8 encoded byte string. String literals have
type `str` but are stored statically and never allocate unless modified.

```astra
var s = "hello"
s.push(", world")
println(s.len())        // 12 (bytes)
let b: u8 = s[0]        // indexing yields bytes
```

## 2.4 Structs

```astra
struct User {
    name: str
    age: int
}
```

Structs are value types: they are stored inline (on the stack or inside
another value), never boxed implicitly. See chapter 3 for declarations.

## 2.5 Enums

Enums are tagged unions (sum types). Each variant may carry data.

```astra
enum Shape {
    Circle(radius: float)
    Rect(width: float, height: float)
    Empty
}
```

A value of an enum type is exactly one of its variants. Enums are inspected
with `match` (chapter 4), which must be exhaustive.

## 2.6 Built-in generic types

The bootstrap version of Astra provides a fixed set of generic types.
User-defined generics are **[planned]**.

| Type | Description |
|------|-------------|
| `List<T>` | owned, growable, contiguous array |
| `Map<K, V>` | owned hash map |
| `Option<T>` | `Some(T)` or `None` |
| `Result<T, E>` | `Ok(T)` or `Err(E)` |
| `Box<T>` | owned heap allocation (needed for recursive types) |

```astra
let xs: List<int> = [1, 2, 3]
var ages = Map<str, int>.new()
let first: Option<int> = xs.first()
```

`T?` is shorthand for `Option<T>`.

## 2.7 Fixed-size arrays

`[T; N]` is an array of exactly `N` elements stored inline.

```astra
let grid: [u8; 4] = [0, 0, 0, 0]
```

## 2.8 References

`&T` is a shared (read-only) reference and `&mut T` is an exclusive
(mutable) reference. References never own the value they point to. Their
rules are described in chapter 5.

## 2.9 Copy and move types

A type is **Copy** if a bitwise copy is a complete, independent copy:

- all primitive types and `()`
- shared references `&T`
- fixed arrays `[T; N]` whose element type is Copy
- structs and enums declared with `copy` whose fields are all Copy

```astra
copy struct Vec2 {
    x: float
    y: float
}
```

All other types (`str`, `List`, `Map`, `Box`, and structs or enums that are
not marked `copy`) are **move** types. Chapter 5 defines what moving means.

## 2.10 Type equivalence

Two types are equal if they have the same name and the same type arguments.
Astra uses nominal typing: two structs with identical fields are different
types.
