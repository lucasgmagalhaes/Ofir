# 13. Memory layout and ABI

This chapter defines how values are represented in memory. Unless a type is
marked `@repr(C)`, layouts may change between compiler versions; only the
properties stated as **guaranteed** can be relied on.

## 13.1 Primitive types

| Type | Size (bytes) | Alignment |
|------|-------------:|----------:|
| `bool`, `i8`, `u8` | 1 | 1 |
| `i16`, `u16` | 2 | 2 |
| `i32`, `u32`, `f32` | 4 | 4 |
| `i64`, `u64`, `f64`, `int`, `uint` | 8 | 8 |
| `()` | 0 | 1 |

`uint` and pointers have the target's pointer size; Ofir supports only
64-bit targets in version 1.

## 13.2 Structs and tuples

- The compiler may reorder fields to minimize padding (default layout).
- `@repr(C)` structs use C layout: declaration order, C padding rules.
  **Guaranteed.**
- `@packed` structs have no padding: each field starts right after the
  previous one and the struct has alignment 1. **Guaranteed.** Their fields
  must be `bool`, numbers or `@packed` structs (E0811), and a field of a
  `@packed` struct cannot be borrowed, with `&`, `&mut` or as a `mut`
  argument (E0812): its address may not be aligned.
- `@align(N)` structs (`N` a power of two up to 4096, the page size) have
  an alignment of at least `N` bytes, and a size that is a multiple of it,
  wherever they are stored: locals, globals, fields, enum payloads and the
  heap. **Guaranteed.** A struct cannot be
  both `@packed` and `@align(N)` (E0808).
- Zero-sized structs occupy no space in their container.

## 13.3 Enums

- An enum with no payloads is represented as the smallest integer tag that
  fits its variants.
- An enum with payloads is a tag followed by a union of the payloads.
- `@repr(C)` applies only to enums without payloads (E0808): such an enum
  is a C `int`, its variants numbered from 0 in declaration order.
  **Guaranteed.**
- **Niche optimization (guaranteed for these types):** `Option<&T>`,
  `Option<Box<T>>` and `Option<fn(...)>` have the same size as the pointer;
  `None` is represented by null.

## 13.4 Built-in types

| Type | Representation |
|------|----------------|
| `&T`, `&mut T`, `*T`, `Box<T>` (sized `T`) | one pointer |
| `&[T]`, `&str` | pointer + length (16 bytes) |
| `&dyn I`, `Box<dyn I>` | data pointer + vtable pointer (16 bytes) |
| `str` | pointer + length + capacity (24 bytes) |
| `List<T>` | pointer + length + capacity (24 bytes) |
| `Map<K, V>` | dense entry list in insertion order + open-addressing index table |
| `fn(A) R`, `move fn(A) R` | code pointer + environment pointer (16 bytes) |
| `extern "C" fn(A) R` | code pointer (8 bytes) |

A `str` built from a literal points to read-only static memory and has
capacity 0; it is copied to the heap on first modification.

## 13.5 Calling convention

- Ofir functions use an unspecified internal calling convention, chosen by
  the compiler (currently LLVM's `fastcc` for non-exported functions).
- `extern "C"` functions use the C ABI of the target. **Guaranteed.**
- Large values passed by value may be passed by hidden pointer; this is
  invisible to the program because moved-from values cannot be observed.
- **Parameter modes** (chapter 5.3) are lowered as follows:

  | Mode | Passed as |
  |------|-----------|
  | borrow, Copy type up to 16 bytes | by value, in registers |
  | borrow, type with interior mutability (chapter 5.8) | pointer, never `readonly` |
  | borrow, other types | pointer, `noalias readonly` |
  | borrow, `[T]` / `str` / `dyn I` | fat pointer: data pointer + length or vtable |
  | `mut` | pointer, `noalias` |
  | `move` | by value; large values by hidden pointer to a caller-owned copy |

- `&mut T` values are `noalias`; `&T` values to types without interior
  mutability are `noalias readonly`.

## 13.6 Program startup and shutdown

1. The runtime entry point initializes the allocator and command line
   arguments, then calls `main`.
2. When `main` returns, all its locals have already been dropped; standard
   output buffers are flushed and the process exits with status 0 (or 1 if
   `main` returned `Err`).
3. Global constants are computed at compile time; there are no global
   constructors.

## 13.7 Allocation

All heap allocation goes through the global allocator in `std.mem`, which
defaults to the C library's `malloc`/`free`. Allocation failure panics.
An allocation for a type whose alignment is above 16 (an `@align(N)`
struct, or a value containing one) is aligned to it: in `Box<T>`,
`List<T>` and closure environments alike.
Custom allocators passed explicitly to collections are **[planned]**.
