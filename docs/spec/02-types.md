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
| `uint` | alias for `u64` (bit manipulation, hashing, C interop) |
| `f32`, `f64` | IEEE-754 floats |
| `float` | alias for `f64` |
| `bool` | `true` or `false` |

**Lengths and indices are `int`**, as in Go: `len()` returns `int` and
indexing takes an `int`. A negative index is simply out of range and panics.
This avoids conversions between signed and unsigned values in everyday code.

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
A string literal can also be used wherever `&str` is expected; it is then a
borrow of static data:

```astra
fn greet(name: &str) { println("hello, {name}") }

greet("Ana")            // no allocation, no `&` needed
```

```astra
var s = "hello"
s.push(", world")
println("{s.len()}")    // 12 (bytes)
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

The standard library provides these generic types, available everywhere
through the prelude. User-defined generics are described in chapter 8.

| Type | Description |
|------|-------------|
| `List<T>` | owned, growable, contiguous array |
| `Map<K, V>` | owned hash map that iterates in insertion order |
| `Option<T>` | `Some(T)` or `None` |
| `Result<T, E>` | `Ok(T)` or `Err(E)` |
| `Box<T>` | owned heap allocation (needed for recursive types) |

```astra
let xs: List<int> = [1, 2, 3]
var ages = Map<str, int>.new()
let first: Option<int> = xs.first()
```

`T?` is shorthand for `Option<T>`.

**Map ordering and hashing.** Iterating a `Map` visits entries in the order
their keys were first inserted (removing a key forgets its position). The
default hasher is deterministic: it uses no random seed, so a program's
output never depends on hash values and repeated runs behave identically.
A seeded, DoS-resistant hasher for maps filled from untrusted input is
**[planned]** (`Map.with_hasher(...)`).

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
A Copy type cannot define a `drop` method.

## 2.10 Type equivalence

Two types are equal if they have the same name and the same type arguments.
Astra uses nominal typing: two structs with identical fields are different
types.

## 2.11 Tuples

A tuple groups a fixed number of values of possibly different types.

```astra
fn div_mod(a: int, b: int) (int, int) {
    return (a / b, a % b)
}

let (q, r) = div_mod(17, 5)
let pair = (1, "one")
println("{pair.0}")
```

Tuples are Copy when all their elements are Copy. The empty tuple is the
unit type `()`.

## 2.12 Slices

A slice is a borrowed view of a contiguous sequence: a pointer and a length.

| Type | Meaning |
|------|---------|
| `&[T]` | read-only view of `T` elements |
| `&mut [T]` | mutable view of `T` elements |
| `&str` | read-only view of UTF-8 bytes |

```astra
let xs = [10, 20, 30, 40]
let middle: &[int] = xs[1..3]     // [20, 30]
let s = "hello, world"
let hello: &str = s[0..5]
```

- `&List<T>` and `&[T; N]` convert implicitly to `&[T]`; `&str` borrowed
  from a `str` is already a slice.
- Slicing is bounds checked. Slicing a `str` panics if a bound is not on a
  UTF-8 character boundary.
- Slices are references, so they follow the rules of chapter 5.

## 2.13 Function types

`fn(A, B) R` is the type of a borrowed function value: a named function or
a closure. `move fn(A, B) R` is an owned function value that can be stored
and returned. See chapter 10.

```astra
fn apply(f: fn(int) int, x: int) int {
    return f(x)
}

fn double(x: int) int { return x * 2 }

let y = apply(double, 21)
```

## 2.14 Type aliases

```astra
type NodeId = u32
type Table = Map<str, List<int>>
```

An alias is another name for the same type, not a new type. To create a
distinct type, wrap it in a struct: `struct NodeId { value: u32 }`.

## 2.15 The never type

`!` is the type of expressions that never produce a value: `panic(...)`,
`return`, `break`, `continue` and infinite loops. It converts to any type,
which makes code like this valid:

```astra
let n = match input {
    .Some(v) => v
    .None => panic("missing input")
}
```

## 2.16 Raw pointers

`*T` and `*mut T` are unchecked pointers, usable only in `unsafe` code
(chapter 11).

## 2.17 Sized and unsized types

Most types have a size known at compile time. Three kinds of types do not:

- `dyn I` (the size depends on the concrete type),
- `[T]` (a sequence of unknown length),
- the text behind a `&str`.

Unsized types can only be used behind a pointer: `&`, `&mut`, `*`, `*mut`
or `Box`. Such pointers are fat (chapter 13.4). Generic type parameters are
always sized; `&T`, `&mut T`, `Box<T>` and raw pointers are the built-in
types that also accept an unsized `T`.
