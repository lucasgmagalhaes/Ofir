# 5. Ownership and borrowing

Astra manages memory without a garbage collector. Every value has exactly one
**owner**; when the owner goes out of scope, the value is destroyed and its
memory is released. References let code use a value without owning it.

The model is deliberately simpler than Rust's: **references are
second-class**. They can be passed to functions and used locally, but they
cannot be stored in structs, collections, or returned from functions.
This removes the need for lifetime annotations entirely, while keeping
memory safety and allowing aggressive optimization.

## 5.1 Ownership

```astra
fn main() {
    let a = "hello"           // a owns the string
    let b = a                 // ownership moves to b
    println(&a)               // error: use of moved value `a`
}
```

- Assigning a move type, passing it by value, or returning it **moves** it.
  The source can no longer be used (unless it is a `var` that is assigned a
  new value).
- Copy types (chapter 2.9) are copied instead of moved.
- To duplicate a move type explicitly, call `.clone()`.
- A value can only be moved out of a place the function owns entirely.
  `let x = list[i]` or `let n = user_ref.name` with a move type is an error;
  use `list.remove(i)`, `list.swap_remove(i)`, `mem.replace(&mut list[i], v)`,
  `mem.take(&mut opt)` or `.clone()` instead.

### Partial and conditional moves

```astra
var v = make_list()
if flag {
    consume(v)
}
println("{v.len()}")   // error: `v` may have been moved
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

## 5.3 References

```astra
fn total(xs: &List<int>) int {         // shared borrow
    var sum = 0
    for x in xs { sum += x }
    return sum
}

fn reset(xs: &mut List<int>) {            // exclusive borrow
    xs.clear()
}

fn main() {
    var nums = [1, 2, 3]
    println("{total(&nums)}")
    reset(&mut nums)
}
```

- `&x` creates a shared reference; `&mut x` an exclusive one. `&mut x`
  requires `x` to be a `var` (or reached through another `&mut`).
- **Temporaries are borrowed automatically.** A temporary value (the result
  of a call, an operator or a literal) passed where `&T` is expected is
  borrowed implicitly and dropped at the end of the statement:
  `println(user.describe())`. Named variables and fields always need an
  explicit `&`, so moving and borrowing stay visible.
- **Implicit reborrowing.** Passing a `&mut T` variable where a `&mut T` is
  expected does not move the reference: it is reborrowed for the duration of
  the call and can be used again afterwards. The same applies to method
  calls through a `&mut` receiver.
- Field access and method calls dereference automatically (`p.x` works when
  `p: &Point`). Explicit dereference `*r` is needed for primitives.
- `*r` on a reference to a move type may only be used to read a Copy field
  or to call a method; it cannot move the value out.

## 5.4 Where references may appear

References are **second-class values**. A reference may appear only as:

1. a function or method parameter;
2. a local binding (`let r = &list`);
3. a loop variable or pattern binding;
4. a temporary inside an expression.

A reference may **not**:

- be stored in a struct or enum field;
- be stored in a `List`, `Map`, `Option`, `Box` or any other container;
- be returned from a function, except as described in 5.6.

Because a reference can never outlive the call or block that created it,
no lifetime annotations are ever needed.

## 5.5 Borrowing rules

At any point in a function, for each value:

- there may be **any number of shared references**, **or**
- **exactly one exclusive reference**,

but never both. While a value is borrowed, it cannot be moved, and while it
is exclusively borrowed it cannot be read or written through its owner.

A borrow lasts from the point where the reference is created to its **last
use** (non-lexical borrows):

```astra
var list = [1, 2, 3]
let r = &list
println("{r.len()}")   // last use of r: the borrow ends here
list.push(4)           // ok
```

```astra
var list = [1, 2, 3]
let r = &list
list.push(4)           // error: `list` is borrowed by `r`
println("{r.len()}")
```

The rules apply to arguments of a single call as well:

```astra
swap(&mut a, &mut a)   // error: `a` is borrowed mutably twice
```

## 5.6 Returning borrows and `ref struct`

Two controlled exceptions make the model practical for accessors and
iterators, still without lifetime annotations.

### Returning a borrow

A function may return a reference (or a `ref struct`, below) if it has
**exactly one** reference parameter, counting the receiver. The result
borrows from that parameter:

```astra
fn (l: &List<T>) first() Option<&T> { ... }
fn (u: &mut User) name_mut() &mut str { ... }

let f = list.first()     // f borrows `list`
list.push(4)             // error: `list` is borrowed by `f`
```

With zero or several reference parameters, returning a borrow is an error.
The caller treats the result like any other local reference: it can be used
and passed down, but not stored or returned further (unless the caller is
itself a function following this rule).

`Option<&T>` and `Result<&T, E>` are allowed as return types under this
rule; they are the only containers that may hold a reference, and only as a
return value or a local binding.

### `ref struct`

A struct declared with `ref` may contain references. It is itself
second-class: it follows every restriction of 5.4 and can be returned only
under the rule above.

```astra
ref struct ListIter<T> {
    list: &List<T>
    index: int
}

fn (l: &List<T>) iter() ListIter<T> {
    return ListIter{list: l, index: 0}
}

fn (it: &mut ListIter<T>) next() Option<&T> {
    ...
}
```

This is how iterators, string splitters and parsers over borrowed text are
written.

## 5.7 Why this is fast

- No garbage collector and no reference counting: allocation and release
  points are known at compile time.
- Since two `&mut` references can never alias, the compiler marks them
  `noalias` for LLVM, enabling optimizations that C compilers cannot
  perform safely.
- Small structs passed as `&T` may be passed by value when it is cheaper;
  the semantics are the same because shared references are read-only.

## 5.8 Shared ownership **[planned]**

For graphs and other structures with shared ownership, the standard library
will provide `Rc<T>` (reference counted) and arena allocators. These are
library types built on top of the rules above, not language features.
