# 5. Ownership and borrowing

Astra manages memory without a garbage collector. Every value has exactly one
**owner**; when the owner goes out of scope, the value is destroyed and its
memory is released.

Most code never deals with references explicitly. **Parameters are borrowed
by default**: a function reads its arguments without taking ownership and
without the caller writing `&`. Two keywords in the signature cover the other
cases: `mut` (the function modifies the argument) and `move` (the function
takes ownership). Call sites look the same in all three cases.

```astra
fn total(xs: List<int>) int { ... }            // reads xs
fn reset(xs: mut List<int>) { xs.clear() }      // modifies the caller's list
fn keep(xs: move List<int>) { ... }             // takes ownership

var nums = [1, 2, 3]
println("{total(nums)}")
reset(nums)
keep(nums)                                      // nums is moved here
```

## 5.1 Ownership

```astra
fn main() {
    let a = "hello"           // a owns the string
    let b = a                 // ownership moves to b
    println(a)                // error: use of moved value `a`
}
```

- Assigning a move type to a variable or field, passing it to a `move`
  parameter, or returning it **moves** it. The source can no longer be used
  (unless it is a `var` that is assigned a new value).
- Copy types (chapter 2.9) are copied instead of moved.
- To duplicate a move type explicitly, call `.clone()`.
- A value can only be moved out of a place the function owns entirely.
  `let x = list[i]`, or moving a field out of a borrowed parameter, is an
  error; use `list.remove(i)`, `list.swapRemove(i)`, `mem.replace(list[i], v)`,
  `mem.take(opt)` or `.clone()` instead.

### Partial and conditional moves

```astra
var v = makeList()
if flag {
    consume(v)          // fn consume(xs: move List<int>)
}
println("{v.len()}")    // error: `v` may have been moved
```

A value that is moved on some paths is considered moved after the branches
join. Moving a field out of a struct is allowed only if the struct is not
used as a whole afterwards (except to reassign the moved field).

## 5.2 Destruction (drop)

When a binding goes out of scope, its value is dropped unless it was moved.
Values are dropped in reverse order of declaration. Dropping a value:

1. calls its `drop` method, if it has one;
2. then drops each field, in declaration order.

Temporaries are dropped at the end of the statement that created them.

If a value is moved on some paths only, the compiler inserts a hidden drop
flag so it is dropped exactly once. This is the only runtime cost of the
model.

## 5.3 Parameter modes

| Declaration | Mode | The function may | The caller passes |
|-------------|------|------------------|-------------------|
| `x: T` | **borrow** (default) | read `x` | any expression; it keeps ownership |
| `x: mut T` | **mutable borrow** | read, modify and reassign `x` | a mutable place (a `var`, a field or element of one, or a `mut` parameter) |
| `x: move T` | **move** | own `x`: store it, return it, drop it | a value, which is moved (or copied, for Copy types) |

The `self` parameter of a method (chapter 3.6) uses the same modes:

```astra
struct Point {
    x: float
    y: float

    fn length(self) float { ... }           // borrows the point
    fn scale(mut self, k: float) { ... }    // modifies it
    fn intoVec2(move self) Vec2 { ... }     // consumes it
}
```

### Borrowed parameters

- A borrowed parameter is read-only: assigning to it, mutating it, or moving
  out of it is an error. To work on a local copy, write `var x = x.clone()`
  (or `var n = n` for Copy types).
- Any expression can be passed: a variable, a field, a literal or the result
  of a call. A temporary lives until the end of the statement, so
  `println(user.describe())` needs nothing special.
- The compiler chooses how to pass the argument: small Copy values in
  registers, everything else by pointer. Since the callee cannot modify it,
  the choice is invisible to the program.

### Mutable parameters

- The caller's place must be mutable: passing a `let` binding or a borrowed
  parameter to a `mut` parameter is an error. Nothing is written at the call
  site: `reset(nums)`.
- Assigning a new value to the whole parameter (`x = T{...}`) replaces the
  caller's value; the old value is dropped.
- Moving out of a `mut` parameter is allowed only if a new value is assigned
  before the function returns (or `mem.replace`/`mem.take` is used).

### Moved parameters

- The callee owns the value: it can store it in a struct or collection,
  return it, or let it be dropped at the end of the function.
- Inside the function, a `move` parameter is an immutable binding; rebind it
  with `var x = x` to modify it.
- If a function tries to store a borrowed parameter, the error suggests the
  fix: *"declare `item: move str`, or call `item.clone()`"*.

### Borrowed views of sequences and text

As borrowed or `mut` parameter types, `[T]` and `str` accept any contiguous
sequence or text:

```astra
fn sum(xs: [int]) int { ... }       // accepts List<int>, [int; N] and slices
fn shout(s: str) str { ... }        // accepts str values, literals and slices

sum(numbers[0..10])
shout("hello")
```

A borrowed `dyn I` parameter accepts any value whose type satisfies `I`
(chapter 9.7): `fn draw(s: dyn Shape)`.

## 5.4 Borrowing rules

While a call runs, its borrowed arguments are **borrows** of the caller's
places: shared borrows for the default mode and exclusive borrows for `mut`.
For each value, at any point:

- there may be **any number of shared borrows**, **or**
- **exactly one exclusive borrow**,

but never both. While a value is borrowed, it cannot be moved, and while it
is exclusively borrowed it cannot be read or written through its owner.

```astra
fn append(dst: mut List<int>, src: List<int>) { ... }

append(a, b)       // ok
append(a, a)       // error: `a` is borrowed mutably and shared at the same time
```

**Borrows are tracked per place**, not per variable: distinct fields of a
struct (and distinct elements of a tuple) can be borrowed independently.
Indexing (`list[i]`) borrows the whole collection.

```astra
lx.interpolations.push(lx.brackets.len())   // ok: two different fields of lx
```

**Two-phase borrows.** In a call, the arguments are evaluated before an
exclusive borrow of `mut self` or of a `mut` argument becomes active, so
`list.push(list.len())` is accepted.

Because `mut` arguments never alias anything else, the compiler tells LLVM
they are `noalias`, enabling optimizations that C compilers cannot perform
safely.

## 5.5 Borrowed bindings

The same borrow modes apply to other bindings that refer to an existing
value instead of owning it:

| Binding | Mode |
|---------|------|
| `for x in list` | borrows each element (Copy elements are copied) |
| `for mut x in list` | borrows each element mutably; `list` must be a mutable place |
| `match` / `if let` on a place that is not owned (a borrowed parameter, a field of one, an element) | bindings borrow the matched parts (Copy parts are copied) |
| `match` on an owned value | bindings take ownership of the parts |

```astra
for mut item in cart.items {
    item.price = item.price * 90 / 100
}
```

## 5.6 Explicit references (advanced)

The types `&T` and `&mut T` still exist for the cases parameter modes do not
cover. They are **second-class**: a reference may be bound to a local
(`let r = &list`), returned from a function under the rule below, or stored
in a `ref struct`, but never stored in an ordinary struct, enum or
container. Because a reference can never outlive the call or block that
created it, no lifetime annotations are ever needed.

```astra
var list = [1, 2, 3]
let r = &list
println("{r.len()}")   // last use of r: the borrow ends here
list.push(4)           // ok
```

A borrow lasts from the point where it is created to its **last use**
(non-lexical borrows).

**Stage 0** checks where references may be stored and returned (E0702),
and that writes go through `&mut`, but does not check borrows against
moves and mutations of what they borrow: that is the borrow checker of the
self-hosted compiler (roadmap 4.7). A function that returns a borrow
receives its source by address, so that the borrow points into the
caller's value.

### Returning a borrow

A function may return a reference (or a `ref struct`, or an `Option`/`Result`
containing one) that borrows from **one** parameter:

- from `self`, if the function is a method taking `self` or `mut self`;
- otherwise from its only borrowed parameter of a non-Copy type.

With no such parameter, or with several candidates, returning a borrow is an
error.

```astra
// in List<T>:  fn first(self) Option<&T> { ... }
// in User:     fn nameMut(mut self) &mut str { ... }

let f = list.first()     // f borrows `list`
list.push(4)             // error: `list` is borrowed by `f`
```

The caller treats the result like any other local reference: it can be used,
passed to functions, and returned further only under the same rule.

### `ref struct`

A struct declared with `ref` may contain references. It is itself
second-class: it can be bound locally, passed as an argument and returned
under the rule above, but not stored in ordinary structs or containers.

```astra
ref struct ListIter<T> {
    list: &List<T>
    index: int

    fn next(mut self) Option<&T> {
        ...
    }
}

// in List<T>:
fn iter(self) ListIter<T> {
    return ListIter{list: &self, index: 0}
}
```

This is how iterators, string splitters and parsers over borrowed text are
written.

## 5.7 Why this is fast

- No garbage collector and no reference counting: allocation and release
  points are known at compile time.
- Borrowed parameters are passed by pointer or by value as the compiler sees
  fit, never copied deeply.
- `mut` parameters and `&mut` references never alias, so they are marked
  `noalias` for LLVM; borrowed parameters of types without interior
  mutability are `noalias readonly`.

## 5.8 Interior mutability

Some types must allow mutation through a shared borrow: counters shared by
several owners, caches, locks. They are built on one primitive,
`UnsafeCell<T>` in `std.mem`, the only way to obtain a `*mut T` from a
shared borrow without undefined behavior. Safe wrappers in the standard
library:

| Type | Use |
|------|-----|
| `Cell<T>` (T Copy) | `get` / `set` a value through a shared borrow |
| `Mutex<T>`, `RwLock<T>`, atomics | shared mutation across threads (chapter 14) |

A type **has interior mutability** if it contains an `UnsafeCell` directly or
through its fields. The compiler never marks borrows of such types as
`readonly` (chapter 13.5). `RefCell` (borrow checking at runtime) is
intentionally not provided.

## 5.9 Shared ownership **[planned]**

For graphs and other structures with shared ownership, the standard library
will provide `Rc<T>` (reference counted) and arena allocators. These are
library types built on top of the rules above, not language features.
