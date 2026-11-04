# 9. Interfaces

An interface names a set of methods. A type **satisfies** an interface when
it has all of those methods. As in Go, satisfaction is **implicit**: there
is no `implements` declaration. Unlike Go, interfaces can be used both with
**static dispatch** (generics, zero cost) and **dynamic dispatch** (`dyn`,
one indirect call).

## 9.1 Declaring interfaces

```ofir
interface Shape {
    fn area(self) float
    fn name(self) str
}
```

- `Self` is the type satisfying the interface.
- Methods are written as inside a struct (chapter 3.6): `self`, `mut self`
  or `move self` as the first parameter.
- Associated functions without `self` are allowed: `fn default() Self`.

## 9.2 Satisfying interfaces

```ofir
struct Circle {
    radius: float

    fn area(self) float {
        return 3.14159 * self.radius * self.radius
    }

    fn name(self) str {
        return "circle"
    }
}

// Circle now satisfies Shape. Nothing else is needed.
```

A method satisfies an interface method when the name, the mode of `self`,
the parameter types and the return type match exactly.

### Explicit assertion

To document intent and get an error at the type's declaration if a method is
missing, a type can assert that it satisfies an interface:

```ofir
struct Circle: Shape + Display {
    radius: float

    ...
}
```

The assertion does not change behavior; it only adds a compile-time check.

## 9.3 Default methods

An interface method can have a body. Types get it for free unless they define
their own:

```ofir
interface Shape {
    fn area(self) float
    fn name(self) str

    fn describe(self) str {
        return "{self.name()} with area {self.area()}"
    }
}
```

A type gets a default method where it is used through a bound, and, as a
method of its own, from the interfaces it asserts (`struct Circle: Shape`)
and those they embed. Satisfying an interface without asserting it does not
add its default methods to the type.

## 9.4 Embedding

An interface can include other interfaces:

```ofir
interface Reader {
    fn read(mut self, buf: mut [u8]) Result<int, IoError>
}

interface Writer {
    fn write(mut self, data: [u8]) Result<int, IoError>
}

interface ReadWriter {
    Reader
    Writer
}
```

## 9.5 Generic interfaces

```ofir
interface Iterator<T> {
    fn next(mut self) Option<T>
}

interface From<T> {
    fn from(value: move T) Self
}
```

Stage 1 does not support generic interfaces yet (E0901).

## 9.6 Static dispatch

An interface used as a bound is resolved at compile time. Calls are direct
and can be inlined:

```ofir
fn totalArea<S: Shape>(shapes: [S]) float {
    var sum = 0.0
    for s in shapes {
        sum += s.area()
    }
    return sum
}
```

## 9.7 Dynamic dispatch with `dyn`

`dyn Interface` is a type whose concrete type is known only at runtime.
It is unsized, so it is used as a parameter type or behind a pointer:

| Use | Meaning |
|-----|---------|
| `s: dyn Shape` (parameter) | borrows any value whose type satisfies `Shape` |
| `s: mut dyn Shape` (parameter) | borrows it mutably |
| `Box<dyn Shape>` | owned, e.g. in a list of different shapes |
| `&dyn Shape` | explicit reference (chapter 5.6) |

```ofir
fn draw(s: dyn Shape) {
    println(s.describe())
}

draw(Circle{radius: 1.0})
```

```ofir
let shapes: List<Box<dyn Shape>> = [
    Box.new(Circle{radius: 1.0}),
    Box.new(Square{side: 2.0}),
]

for s in shapes {
    println(s.describe())
}
```

A `dyn` value is a **fat pointer**: a data pointer plus a pointer to a
virtual table with one entry per method, plus the size, alignment and drop
function of the concrete type. Converting a concrete pointer to a `dyn`
pointer is implicit when the expected type is known.

### Dyn compatibility

An interface can be used with `dyn` only if every method:

- takes `self` or `mut self` (no `move self`, no associated
  functions);
- has no type parameters of its own;
- does not mention `Self` in parameter or return types.

Methods that break these rules may still exist if they have a default
body; they are simply not callable through `dyn`.

## 9.8 Type switches

The concrete type behind a `dyn` value can be tested with `match`:

```ofir
match shape {
    c: Circle => println("circle of radius {c.radius}")
    _ => println("another shape")
}
```

This requires the concrete type to be known to the compiler at the match
site; the vtable carries a unique type id. A type switch needs a `_` arm,
since the set of types behind a `dyn` is open (E0500). A type test that
names a type that does not satisfy the interface can never match (E0354).
A type test binds like any pattern (chapter 5.5): on a borrowed `dyn`
value the binding borrows; on an owned `Box<dyn I>` it takes the value by
move and the box is freed.

### `Any`

`Any` (ADR 0024), in `std.core` and the prelude, holds a value of any
type:

```ofir
pub interface Any {
    fn typeName(self) str
}
```

Every type satisfies `Any`, except the types that hold references (`&T`,
a `ref struct`, a type with such a field), which cannot be stored (chapter
5.6, E0702). The compiler provides `typeName` for every type, which cannot
declare its own; it returns the type's name as written in messages:
`"int"`, `"List<str>"`, `"app.Config"`. The value is recovered with a type
switch:

```ofir
fn describe(v: dyn Any) str {
    return match v {
        n: int => "the int {n}"
        s: str => "the str {s}"
        p: Point => "a point at {p.x}, {p.y}"
        _ => "a {v.typeName()}"
    }
}

fn takeConfig(value: move Box<dyn Any>) Option<Config> {
    return match value {
        c: Config => .Some(c)   // moved out of the box
        _ => .None              // the box is dropped with its value
    }
}
```

`if let c: Config = value { ... }` tests a single type. There is no `any`
type, no `is` operator and no reflection: a type test, which the compiler
checks, is the only way to the concrete value. Two `dyn Any` values cannot
be compared or hashed.

## 9.9 Built-in interfaces

These interfaces are defined in `std.core` and are part of the prelude.
The compiler **derives them automatically** for structs and enums whose
fields all satisfy them, unless the type defines the method itself.

| Interface | Methods | Used by |
|-----------|---------|---------|
| `Eq` | `fn eq(self, other: Self) bool` | `==`, `!=` |
| `Ord` | `fn cmp(self, other: Self) Ordering` | `<`, `<=`, `>`, `>=`, sorting |
| `Hash` | `fn hash(self, h: mut Hasher)` | `Map` keys |
| `Clone` | `fn clone(self) Self` | `.clone()` |
| `Display` | `fn format(self, out: mut str)` | string interpolation, `println` |
| `Debug` | `fn debug(self, out: mut str)` | `{x:?}` in interpolation |
| `Drop` | `fn drop(mut self)` | destruction (chapter 5.2) |
| `Iterator<T>` | `fn next(mut self) Option<T>` | `for` loops |
| `From<T>` | `fn from(value: move T) Self` | error conversion in `?` (chapter 6.2) |
| `Default` | `fn default() Self` | `..T.default()` in struct literals, `mem.take` |

`Display` is **not** derived automatically; a type chooses how it is shown.
`Debug` is derived for every type.

A `for` loop accepts built-in collections (`List`, arrays, slices, `str`,
`Map`) and any value whose type satisfies `Iterator<T>`. User collections
expose iteration through a method returning a `ref struct` iterator
(chapter 5.6), e.g. `for x in tree.iter() { }`.

Operator overloading beyond these interfaces (for example `+` on user
types) is intentionally not supported: an operator never hides a function
call that the reader cannot predict.

## 9.10 Coherence

Methods are declared inside the body of their type (chapter 3.6), so a
type's methods are all known where the type is declared. As a consequence, whether a type satisfies an interface never depends
on which modules are imported.
