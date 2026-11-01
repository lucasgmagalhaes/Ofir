# 8. Generics

Generics let functions and types work over many types without losing static
typing or performance. Astra implements them by **monomorphization**: each
distinct instantiation is compiled to specialized code, so generic code is
as fast as hand-written code.

## 8.1 Generic functions

```astra
fn max<T: Ord>(a: move T, b: move T) T {
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

The type parameters of a generic type are in scope in all of its methods:

```astra
struct Stack<T> {
    items: List<T>

    fn new() Stack<T> {
        return Stack{items: []}
    }

    fn push(mut self, item: move T) {
        self.items.push(item)
    }

    fn pop(mut self) Option<T> {
        return self.items.pop()
    }
}
```

Methods may add their own type parameters and bounds:

```astra
fn contains(self, item: T) bool where T: Eq { ... }
fn map<U>(self, f: fn(T) U) Stack<U> { ... }
```

## 8.4 `where` clauses

Bounds may be written after the signature when they are long:

```astra
fn merge<K, V>(a: move Map<K, V>, b: move Map<K, V>) Map<K, V>
where
    K: Eq + Hash,
    V: Clone,
{
    ...
}
```

## 8.5 Explicit type arguments

Type arguments are normally inferred from arguments and from the expected
type. In order, they come from what is written, the receiver (`s.push(x)`
on a `Stack<int>`), the expected type, then the value arguments from left
to right; the first type found for a parameter is its argument, and the
arguments are then checked against the substituted signature. A parameter
that none of them determines is an error (E0330). They can be written
explicitly:

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
