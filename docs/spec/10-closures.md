# 10. Closures

A closure is an anonymous function that can capture variables from the
scope where it is created.

## 10.1 Syntax

```astra
let double = fn(x: int) int { return x * 2 }
let add = fn(a: int, b: int) int => a + b         // expression body
let names = users.map(fn(u) => u.name.clone())    // types inferred
let greet = move fn() => println("hi {name}")     // captures by value
```

```ebnf
Closure      = [ "move" ] "fn" "(" [ ClosureParam { "," ClosureParam } ] ")"
               [ Type ] ( Block | "=>" Expr ) .
ClosureParam = identifier [ ":" Type ] .
```

Parameter and return types can be omitted when they can be inferred from
the expected function type.

## 10.2 Two function types

| Type | Meaning | Second-class? | Copy? |
|------|---------|:-------------:|:-----:|
| `fn(A) R` | a **borrowed** function value | yes | yes |
| `move fn(A) R` | an **owned** function value | no | no (move) |

- `fn(A) R` behaves like a reference (chapter 5.4): it can be called, bound
  locally and passed as an argument, but **not stored in a struct or
  container, and not returned**. Every function and every closure converts
  to it. This is the type of callbacks: `map`, `filter`, `sort_by`, `each`.
- `move fn(A) R` owns its captured values. It can be stored, returned and
  sent across the program. Only named functions and `move` closures convert
  to it. It is a move type; dropping it drops its captured values.

Because function-typed parameters are borrowed by default, a function that
receives a callback can never store it by accident; one that needs to store
it says so in its signature:

```astra
fn each(xs: &List<int>, f: fn(int)) { ... }            // cannot keep `f`

fn on_click(b: &mut Button, handler: move fn()) {      // keeps `handler`
    b.handlers.push(handler)
}
```

## 10.3 Captures

A non-`move` closure captures each variable it uses by reference, chosen
from how the body uses it:

| Use in body | Capture |
|-------------|---------|
| only read | shared reference `&` |
| modified | exclusive reference `&mut` |

```astra
var count = 0
let increment = fn() { count += 1 }    // captures `count` by &mut
increment()
increment()
println("{count}")                      // 2: the borrow ended at the last call
```

The captured borrows follow the borrowing rules of chapter 5: while
`increment` is alive (until its last use), `count` cannot be used directly.

A `move` closure captures each variable it uses **by value**: Copy values
are copied, other values are moved into the closure. Captured values are
**immutable** inside a `move` closure; mutable state that must outlive a
call belongs in a struct with methods.

```astra
fn make_greeter(name: str) move fn() str {
    return move fn() str => "hello, {name}"
}
```

## 10.4 Representation

- A function value is a pair: a code pointer and an environment pointer.
  Named functions use a null environment.
- A non-`move` closure stores its captured references in an environment on
  the creator's stack: no allocation.
- A `move` closure stores its captured values in a heap-allocated
  environment, freed when the closure is dropped. Closures capturing nothing
  do not allocate.
- When a function receiving a callback is inlined (or generic and
  monomorphized), a closure passed directly to it is inlined too.
- A `move fn` is `Send` (chapter 14) when all its captured values are `Send`.
