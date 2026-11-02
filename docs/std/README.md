# Standard library reference

This reference lists the public API of the Ofir standard library. It is
normative: the compilers and `std/` implement exactly these signatures.

Entries marked **S0** must be provided by stage 0 (see `docs/bootstrap.md`);
in stage 0 several of them are built into the compiler instead of being
written in Ofir. Entries marked **[planned]** are designed but not
required before version 1.0.

| Module | Reference |
|--------|-----------|
| prelude, `std.core` | [core.md](core.md): `Option`, `Result`, `Ordering`, interfaces, numbers |
| `std.collections` | [collections.md](collections.md): `List`, `Map`, `Box` |
| `std.strings` | [strings.md](strings.md): `str` methods and helpers |
| `std.io`, `std.fs`, `std.os` | [system.md](system.md): input/output, files, processes |
| `std.mem`, `std.math`, `std.testing`, `std.fmt` | [utilities.md](utilities.md) |

## Conventions

- Methods that can fail return `Result` or `Option`; panicking variants
  are named explicitly (`unwrap`, `expect`) or documented with **panics**.
- Methods that allocate say so.
- `len()` and indices are `int` (spec 2.1).
- Methods are listed with their `self` parameter, as declared inside the
  type (spec 3.6); functions without `self` are associated functions, called
  as `List.new()`.
- Parameters are borrowed unless marked `mut` or `move` (spec 5.3); methods
  that store or consume an argument take it with `move`.
- Errors from the system are values of `IoError` (see [system.md](system.md)).
