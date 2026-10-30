# 10. Closures

A closure is an anonymous function that can capture variables from the
scope where it is created.

## 10.1 Syntax

```astra
let double = fn(x: int) int { return x * 2 }
let add = fn(a: int, b: int) int => a + b      // expression body
let names = users.map(fn(u) => u.name.clone())    // types inferred
```

```ebnf
Closure     = "fn" "(" [ ClosureParams ] ")" [ Type ] ( Block | "=>" Expr ) .
ClosureParam = identifier [ ":" Type ] .
```

Parameter and return types can be omitted when they can be inferred from
the expected function type.

## 10.2 Captures

A closure captures each variable it uses in one of three ways, chosen by the
compiler from how the variable is used inside the body:

| Use in body | Capture |
|-------------|---------|
| only read | by shared reference |
| modified | by exclusive reference |
| moved, or closure marked `move` | by value (ownership moves into the closure) |

```astra
var count = 0
let increment = fn() { count += 1 }    // captures `count` by &mut
increment()
increment()
println(count)                          // 2

let name = "Astra"
let greet = move fn() => println("hi {name}")   // `name` moved in
```

## 10.3 Escaping and non-escaping closures

The capture rules interact with second-class references (chapter 5):

- A closure that captures **any reference** is itself **second-class**: it
  may be called, bound locally and passed as an argument, but not stored in
  a struct or container nor returned. This is the common case for callbacks
  such as `map`, `filter` and `sort_by`.
- A closure that captures **only by value** (or nothing) is a normal owned
  value: it can be stored, returned and moved across the program.

```astra
fn make_counter() fn() int {
    var n = 0
    return move fn() int {
        n += 1
        return n
    }
}
```

## 10.4 Representation

- A function value `fn(A) R` is a pair: a code pointer and an environment
  pointer. Named functions use a null environment.
- Non-escaping closures keep their environment on the caller's stack: no
  allocation.
- An escaping closure stores its captured values in a heap-allocated
  environment that is freed when the closure is dropped.
- When a closure is passed to a generic parameter (`F: Fn(int) int`
  **[planned]**), it can be inlined completely.
