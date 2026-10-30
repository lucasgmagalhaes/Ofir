# 8. Generics

Generics let functions and types work over many types without losing static
typing or performance. Astra implements them by **monomorphization**: each
distinct instantiation is compiled to specialized code, so generic code is
as fast as hand-written code.

## 8.1 Generic functions

```astra
fn max<T: Ord>(a: T, b: T) T {
    if a > b {
        return a
    }
    return b
}

let m = max(3, 7)           // T inferred as int
let s = max("a", "b")       // T inferred as str
```

```ebnf
TypeParams = "<" TypeParam { "," TypeParam } [ "," ] ">" .
TypeParam  = identifier [ ":" Bounds ] .
Bounds     = TypeRef { "+" TypeRef } .
```

A type parameter can only be used in ways allowed by its bounds. Without
bounds, a value of type `T` can only be moved, dropped, borrowed and passed
to other generic code.

## 8.2 Generic types

```astra
struct Stack<T> {
    items: List<T>
}

enum Tree<K: Ord, V> {
    Leaf
    Node(key: K, value: V, left: Box<Tree<K, V>>, right: Box<Tree<K, V>>)
}
```

## 8.3 Methods on generic types

The receiver's type parameters are in scope in the method:

```astra
fn Stack<T>.new() Stack<T> {
    return Stack{items: []}
}

fn (s: &mut Stack<T>) push(item: T) {
    s.items.push(item)
}

fn (s: &mut Stack<T>) pop() Option<T> {
    return s.items.pop()
}
```

Methods may add their own type parameters and bounds:

```astra
fn (s: &Stack<T>) contains(item: &T) bool where T: Eq { ... }
fn (s: &Stack<T>) map<U>(f: fn(&T) U) Stack<U> { ... }
```

## 8.4 `where` clauses

Bounds may be written after the signature when they are long:

```astra
fn merge<K, V>(a: Map<K, V>, b: Map<K, V>) Map<K, V>
where
    K: Eq + Hash,
    V: Clone,
{
    ...
}
```

## 8.5 Explicit type arguments

Type arguments are normally inferred from arguments and from the expected
type. They can be written explicitly:

```astra
let xs = List<int>.new()
let n = parse<u16>("8080")?
let size = sizeOf<Point>()
```

**Disambiguation.** Because comparison operators do not chain (chapter
4.8), `a < b > c` is never a valid comparison. In expressions, `Name<...>`
is parsed as type arguments when the tokens between `<` and `>` form a valid
type list **and** the `>` is immediately followed by `(`, `.` or `{`.
Otherwise `<` is the less-than operator. In the rare case where an argument
list really contains `a < b, c > (d)`, parenthesize the comparison.

## 8.6 Instantiation rules

- Every instantiation must be resolvable at compile time; there is no
  runtime type information except for `dyn` (chapter 9).
- Recursive instantiation that would generate infinitely many types
  (`fn f<T>() { f<List<T>>() }`) is a compile error, detected by a fixed
  depth limit (64).
- Bounds are checked at the definition, not at each instantiation: a generic
  function that compiles is valid for every type satisfying its bounds.
  This gives clear errors, unlike C++ templates.

## 8.7 Const generics **[planned]**

```astra
struct Matrix<const R: int, const C: int> {
    cells: [float; R * C]
}
```
