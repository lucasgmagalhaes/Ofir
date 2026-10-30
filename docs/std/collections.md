# `std.collections`: `List`, `Map`, `Box`

These types are in the prelude.

## `List<T>`

A growable array. Literals: `[1, 2, 3]`, `[0; 16]` (16 zeros), `[]` (type
from context).

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn List<T>.new() List<T>` | empty, no allocation | ✓ |
| `fn List<T>.with_capacity(n: int) List<T>` | allocates | ✓ |
| `fn (l: &List<T>) len() int` | | ✓ |
| `fn (l: &List<T>) is_empty() bool` | | ✓ |
| `fn (l: &mut List<T>) push(x: T)` | may allocate | ✓ |
| `fn (l: &mut List<T>) pop() Option<T>` | | ✓ |
| `fn (l: &mut List<T>) insert(i: int, x: T)` | **panics** if `i > len` | ✓ |
| `fn (l: &mut List<T>) remove(i: int) T` | shifts later elements; **panics** if out of range | ✓ |
| `fn (l: &mut List<T>) swap_remove(i: int) T` | O(1), does not preserve order | ✓ |
| `fn (l: &mut List<T>) clear()` | drops all elements | ✓ |
| `fn (l: &mut List<T>) truncate(n: int)` | | ✓ |
| `fn (l: &mut List<T>) extend(other: List<T>)` | moves the elements of `other` | ✓ |
| `fn (l: &List<T>) get(i: int) Option<&T>` | returns a borrow (spec 5.6) | ✓ |
| `fn (l: &mut List<T>) get_mut(i: int) Option<&mut T>` | | ✓ |
| `fn (l: &List<T>) first() Option<&T>`, `last()` | | ✓ |
| `fn (l: &List<T>) contains(x: &T) bool where T: Eq` | | ✓ |
| `fn (l: &mut List<T>) sort() where T: Ord` | stable; S0 only for integers, `str` and tuples of those | ✓ |
| `fn (l: &mut List<T>) sort_by(cmp: fn(&T, &T) Ordering)` | stable | |
| `fn (l: &mut List<T>) reverse()` | | ✓ |
| `fn (l: &List<T>) iter() ListIter<T>` | borrowing iterator | |
| `fn (l: &List<T>) map<U>(f: fn(&T) U) List<U>` | allocates | |
| `fn (l: &List<T>) filter(f: fn(&T) bool) List<T> where T: Clone` | allocates | |
| `fn (l: &List<T>) join(sep: &str) str where T: Display` | allocates | ✓ |

Indexing `l[i]` and slicing `l[a..b]` are bounds checked (spec 4.9).
`&List<T>` converts to `&[T]`; slice methods (`len`, `get`, `first`, `last`,
`contains`, `iter`) are shared. `fn (s: &[T]) to_list() List<T> where T: Clone`
copies a slice into a new list (S0 for Copy element types).

## `Map<K, V>`

A hash map iterating in insertion order (spec 2.6). Requires `K: Eq + Hash`.

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn Map<K, V>.new() Map<K, V>` | | ✓ |
| `fn (m: &Map<K, V>) len() int` | | ✓ |
| `fn (m: &mut Map<K, V>) insert(k: K, v: V) Option<V>` | returns the previous value | ✓ |
| `fn (m: &Map<K, V>) get(k: &K) Option<&V>` | | ✓ |
| `fn (m: &mut Map<K, V>) get_mut(k: &K) Option<&mut V>` | | ✓ |
| `fn (m: &Map<K, V>) contains(k: &K) bool` | | ✓ |
| `fn (m: &mut Map<K, V>) remove(k: &K) Option<V>` | | ✓ |
| `fn (m: &mut Map<K, V>) get_or_insert(k: K, default: V) &mut V` | | ✓ |
| `fn (m: &Map<K, V>) keys() MapKeys<K, V>`, `values()` | borrowing iterators | |

`for k, v in map` iterates entries (spec 4.4). With `K = str`, lookups
accept `&str`, including string literals: `ages.get("ana")`.

## `Box<T>`

An owned heap allocation.

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn Box<T>.new(x: T) Box<T>` | allocates | ✓ |
| `fn (b: Box<T>) unbox() T` | moves the value out and frees the box | ✓ |

`Box<T>` dereferences automatically for field access and method calls;
`*b` reads the value.
