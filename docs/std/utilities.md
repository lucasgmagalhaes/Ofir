# `std.mem`, `std.math`, `std.testing`, `std.fmt`

## `std.mem`

| Item | Description | S0 |
|------|-------------|:--:|
| `fn swap<T>(a: &mut T, b: &mut T)` | | ✓ |
| `fn replace<T>(dest: &mut T, value: T) T` | returns the old value | ✓ |
| `fn take<T: Default>(dest: &mut T) T` | leaves `T.default()` behind | |
| `fn size_of<T>() int`, `fn align_of<T>() int` | also in the prelude | ✓ |
| `struct UnsafeCell<T>` | the interior mutability primitive (spec 5.8) | |
| `struct Cell<T>` (T Copy) with `get`, `set` | | |
| `unsafe fn alloc(size: int, align: int) *mut u8`, `free`, `realloc` | raw allocation | ✓ |

## `std.math`

Constants `PI`, `E`, `TAU`; functions `hypot(x, y)`, `min`, `max` over
floats. Most operations are methods on the float types (core.md).

## `std.testing`

| Item | Description |
|------|-------------|
| `assert_eq(left, right)` | in the prelude (spec 7.5) |
| `fn assert_ne<T: Eq + Debug>(left: &T, right: &T)` | |
| `struct Bencher` with `fn (b: &mut Bencher) run(f: fn())` | used by `@bench` |

## `std.fmt`

Implements interpolation. Users normally only implement `Display.format`
(spec 9.9) and call `out.push(...)` on the `&mut str` they receive. Format
specifiers (spec 1.7) are applied by `std.fmt` after the value is formatted.
