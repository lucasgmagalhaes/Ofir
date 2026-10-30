# Prelude and `std.core`

Everything on this page is available without imports.

## `Option<T>`

```astra
enum Option<T> { Some(value: T)  None }
```

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn (o: &Option<T>) isSome() bool` | | ✓ |
| `fn (o: &Option<T>) isNone() bool` | | ✓ |
| `fn (o: Option<T>) unwrap() T` | **panics** on `None` | ✓ |
| `fn (o: Option<T>) expect(msg: &str) T` | **panics** with `msg` on `None` | ✓ |
| `fn (o: Option<T>) unwrapOr(default: T) T` | | ✓ |
| `fn (o: &Option<T>) asRef() Option<&T>` | borrow the content | ✓ |
| `fn (o: &mut Option<T>) take() Option<T>` | leaves `None` behind | ✓ |
| `fn (o: Option<T>) okOr<E>(err: E) Result<T, E>` | | ✓ |
| `fn (o: Option<T>) map<U>(f: fn(T) U) Option<U>` | | |
| `fn (o: Option<T>) unwrapOrElse(f: fn() T) T` | | |

## `Result<T, E>`

```astra
enum Result<T, E> { Ok(value: T)  Err(error: E) }
```

| Method | Description | S0 |
|--------|-------------|:--:|
| `fn (r: &Result<T, E>) isOk() bool` | | ✓ |
| `fn (r: &Result<T, E>) isErr() bool` | | ✓ |
| `fn (r: Result<T, E>) unwrap() T` | **panics** on `Err` (requires `E: Debug`) | ✓ |
| `fn (r: Result<T, E>) expect(msg: &str) T` | **panics** with `msg` | ✓ |
| `fn (r: Result<T, E>) unwrapOr(default: T) T` | | ✓ |
| `fn (r: Result<T, E>) ok() Option<T>` | | ✓ |
| `fn (r: Result<T, E>) mapErr<F>(f: fn(E) F) Result<T, F>` | | |
| `fn (r: Result<T, E>) map<U>(f: fn(T) U) Result<U, E>` | | |

**Error conversion in `?`** (stage 1): if the function returns
`Result<_, F>` and `F` satisfies `From<E>`, `?` converts the error with
`F.from(e)`. Otherwise both error types must be equal.

## `Ordering`

```astra
copy enum Ordering { Less  Equal  Greater }
```

Methods: `isLt()`, `isEq()`, `isGt()`, `reverse()`, and
`then(other: Ordering) Ordering` for lexicographic comparison.

## Interfaces

Defined in spec chapter 9.9: `Eq`, `Ord`, `Hash`, `Clone`, `Display`,
`Debug`, `Drop`, `Iterator<T>`, `Default`, `From<T>`, and the markers `Send`
and `Sync`.

```astra
interface Hash {
    fn (&Self) hash(h: &mut Hasher)
}

struct Hasher { ... }                   // deterministic (spec 2.6)
fn (h: &mut Hasher) write(bytes: &[u8])
fn (h: &mut Hasher) writeInt(v: int)
fn (h: &Hasher) finish() u64
```

## Integer methods

For every integer type `I`:

| Method | Description | S0 |
|--------|-------------|:--:|
| `wrappingAdd/Sub/Mul(b: I) I` | modular arithmetic | ✓ |
| `checkedAdd/Sub/Mul/Div(b: I) Option<I>` | `None` on overflow | ✓ |
| `saturatingAdd/Sub/Mul(b: I) I` | clamps to the range | |
| `wrappingCast<T>()`, `saturatingCast<T>()`, `checkedCast<T>()` | spec 2.1 | ✓ |
| `divFloor(b: I) I`, `remEuclid(b: I) I` | other division conventions | |
| `abs() I` | **panics** on `MIN` for signed types | ✓ |
| `min(b: I) I`, `max(b: I) I`, `clamp(lo: I, hi: I) I` | | ✓ |
| `pow(exp: u32) I` | checked | ✓ |
| `countOnes() int`, `leadingZeros() int`, `trailingZeros() int` | | |
| `I.MIN`, `I.MAX` | constants | ✓ |
| `I.parse(s: &str) Result<I, ParseError>` | decimal, optional sign | ✓ |

## Float methods

For `f32` and `f64`: `sqrt`, `abs`, `floor`, `ceil`, `round`, `trunc`,
`powi(n: i32)`, `powf`, `exp`, `ln`, `log10`, `sin`, `cos`, `tan`, `atan2`,
`min`, `max`, `isNan`, `isInfinite`, `totalCmp(other) Ordering`,
`F.parse(s: &str) Result<F, ParseError>`, and the constants `F.NAN`,
`F.INFINITY`, `F.EPSILON`. In stage 0: `sqrt`, `abs`, `floor`, `ceil`,
`isNan`, `parse`.
