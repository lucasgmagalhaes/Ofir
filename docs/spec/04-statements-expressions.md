# 4. Statements and expressions

## 4.1 Blocks

A block `{ ... }` is a sequence of statements and opens a new scope. When a
block is used as a value (in an `if` or `match` expression), its value is its
last expression.

## 4.2 Assignment

```astra
x = 10
x += 1
user.age = 31
items[0] = 5
```

The left side must be a mutable place: a `var` binding, a field or element
of a mutable place, or a place reached through a `&mut` reference.
Assignment is a statement, not an expression.

## 4.3 `if`

```astra
if x > 0 {
    println("positive")
} else if x < 0 {
    println("negative")
} else {
    println("zero")
}

let sign = if x >= 0 { 1 } else { -1 }
```

The condition must be `bool` (no truthiness). When `if` is used as a value,
`else` is mandatory and both branches must have the same type.

## 4.4 `for`

`for` is the only loop.

```astra
for {                      // infinite loop
    if done() { break }
}

for n < 100 {              // while-style
    n *= 2
}

for i in 0..10 { }         // 0, 1, ..., 9
for i in 0..=10 { }        // 0, 1, ..., 10

for item in list { }       // item: &T  (borrows the list)
for item in &mut list { }  // item: &mut T
for i, item in list { }    // with the index (uint)

for key, value in map { }  // key: &K, value: &V
```

`break` and `continue` apply to the innermost loop. Labeled loops are
**[planned]**.

## 4.5 `match`

```astra
let area = match shape {
    .Circle(r) => 3.14159 * r * r
    .Rect(w, h) => w * h
    .Empty => 0.0
}

match code {
    200 => println("ok")
    301 | 302 => println("redirect")
    400..=499 => println("client error")
    _ => println("other")
}
```

- Arms are separated by newlines. An arm's body is an expression or a block.
- Patterns: literals, ranges (`a..=b`), `_`, bindings (`x`), enum variants
  with sub-patterns (`.Some(x)`), struct patterns (`Point{x, y: 0}`) and
  alternatives (`p | q`).
- A guard may follow a pattern: `.Some(n) if n > 0 => ...`
- `match` **must be exhaustive**; a missing case is a compile error that
  lists the missing patterns.
- Matching an owned value moves its fields into the bindings.
- Matching through a reference binds Copy fields by value and other fields
  by reference: matching a `&Shape` gives `r: float` for `.Circle(r)`, but
  `name: &str` for a `str` field.

## 4.6 `if let`

A shorthand for a `match` with one interesting arm:

```astra
if let .Some(user) = find_user(id) {
    println(user.name)
} else {
    println("not found")
}
```

## 4.7 `return`, `break`, `continue`

```astra
return            // in functions returning ()
return value
```

## 4.8 Operators and precedence

From highest to lowest precedence:

| Precedence | Operators | Associativity |
|-----------:|-----------|---------------|
| 9 | `.` field/method, `()` call, `[]` index, `?` | left |
| 8 | unary `-`, `!`, `&`, `&mut`, `*` (deref) | right |
| 7 | `as` | left |
| 6 | `*`, `/`, `%`, `<<`, `>>`, `&` | left |
| 5 | `+`, `-`, `\|`, `^` | left |
| 4 | `==`, `!=`, `<`, `<=`, `>`, `>=` | none |
| 3 | `&&` | left |
| 2 | `\|\|` | left |
| 1 | `..`, `..=` | none |

Comparison operators do not chain: `a < b < c` is a compile error.

Bitwise operators sit at the same level as the matching arithmetic
operators, as in Go, so `a & mask == 0` means `(a & mask) == 0`.

Arithmetic and comparison operators require both operands to have the same
type. `+` also concatenates `str` values (allocating a new string).
`==` and `!=` work on primitives, `str`, and on structs and enums whose
fields all support `==`.

## 4.9 Indexing

`list[i]`, `array[i]` and `s[i]` (bytes of a string) are bounds checked and
panic when `i` is out of range. The index type is `uint`; integer literals
adapt automatically. `list.get(i)` returns `Option<&T>` instead of panicking.

Slicing (`list[a..b]`) produces a borrowed view and is **[planned]**.

## 4.10 Order of evaluation

Operands, arguments and struct literal fields are evaluated left to right.
