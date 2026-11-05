# 11. Unsafe code and FFI

Most Ofir programs never need this chapter. `unsafe` exists so that the
standard library (allocation, `List`, `Map`, system calls) can be written in
Ofir itself, and so programs can call C libraries.

## 11.1 `unsafe` blocks and functions

```ofir
unsafe {
    let p = alloc(sizeOf<int>() * 4) as *mut int
    *p = 42
}

unsafe fn copyBytes(dst: *mut u8, src: *u8, n: uint) { ... }
```

Inside an `unsafe` block or function, and only there, code may:

1. dereference raw pointers;
2. call `unsafe` functions, including all `extern` functions;
3. convert between pointer types and between pointers and `uint`;
4. read fields of `union` types **[planned]**;
5. access mutable global variables.

Everything else (types, ownership, borrowing) is still checked inside
`unsafe`. A safe function containing an `unsafe` block promises that it
cannot cause undefined behavior for any input; this is the author's
responsibility.

## 11.2 Raw pointers

| Operation | Example |
|-----------|---------|
| from reference (safe) | `let p = &x as *int` |
| null pointer (safe) | `let p: *u8 = nullPtr()` |
| dereference (unsafe) | `*p` |
| offset (unsafe) | `p.add(3)` |
| compare (safe) | `p == q` |

Raw pointers are Copy, carry no ownership and are not tracked by the borrow
checker.

## 11.3 Calling C

```ofir
extern "C" {
    fn strlen(s: *u8) uint
    fn getenv(name: *u8) *u8
}

fn cLen(s: str) uint {
    let c = s.toCString()          // owned, null-terminated copy
    return unsafe { strlen(c.ptr()) }
}
```

- Only types with a defined C layout may cross the boundary: integers,
  floats, `bool`, raw pointers, `@repr(C)` structs and function pointers
  declared `extern "C" fn`.
- The program links against the C library by default. Other libraries are
  linked with `@link("name")` on the `extern` block: the linker gets
  `-lname`, once per library, in the order the blocks are declared.
  `name` is a library name: letters, digits, `_`, `.`, `+` and `-`, not
  starting with `-` (E0809).

```ofir
@link("m")
extern "C" {
    fn cbrt(x: f64) f64
}
```

## 11.4 Exporting functions to C

```ofir
@export("ofir_add")
extern "C" fn add(a: i32, b: i32) i32 {
    return a + b
}
```

- An `extern "C" fn` with a body is a C symbol named after the function;
  `@export("name")` names the symbol `name` instead, a C identifier
  (E0809). Ofir code still calls the function by its own name.
- `@export` applies only to an `extern "C" fn` with a body (E0808).
- A program defines each C symbol once: two `extern "C"` functions with the
  same symbol are an error (E0813).

## 11.5 Mutable globals

```ofir
var COUNTER: int = 0          // mutable global: access requires unsafe
const LIMIT: int = 10         // constants are always safe
```

## 11.6 Undefined behavior

Safe Ofir code cannot cause undefined behavior. Unsafe code causes undefined
behavior if it:

- dereferences a null, dangling or misaligned pointer;
- creates two live `&mut` to the same value, or a `&mut` and a `&` to it;
- reads uninitialized memory;
- produces an invalid value (a `bool` other than 0 or 1, an invalid enum tag,
  a `str` with invalid UTF-8);
- causes a data race.
