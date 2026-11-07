# 0035. Compile-time code checked once and expanded per instance

- Status: Accepted
- Date: 2026-11-07

## Context

ADR 0017 chose compile-time evaluation and type information over macros,
and chapter 15 of the specification fixed the surface: `comptime`
expressions and functions, `std.meta`, `comptime for`, `comptime if`,
`comptime panic` and `@tag`. Its text describes a Zig-like model: the body
of a `comptime for` is type-checked once per element with its concrete
types, and a `comptime if` branch that is not taken is never checked.

Stage 1 does not check generic code per instance. The checker runs once
over each body, with type parameters as opaque types bounded by their
interfaces, and records one type per AST node (`Checker.nodeTypes`). The
lowering to AIR runs once per instance: it knows the instance's type
arguments, chooses the fields of `value[key]` for each key (spec 16.2) and
substitutes the arguments into the body (`monoBody`). Checking a body per
instance would need per-instance node tables in the checker, the borrow
checker and the move analysis, and would report errors only for the
instances a program happens to use.

The key types of chapter 16 already give a body a symbolic field: a
parameter `K: keyof T` is erased, `value[key]` has type `T[K]`, and each
call is instantiated for its key.

## Decision

Compile-time code is **checked once, like any generic code, and expanded
per instance during lowering**. Errors in it are found at the definition,
not at the instantiations.

- **`comptime if C`.** `C` is a compile-time condition: `true`, `false`,
  `const` values, the `std.meta` predicates, `field.index` comparisons,
  `!`, `&&` and `||` (E0363 otherwise). When `C` does not depend on a type
  parameter, only the taken branch is checked. Otherwise both branches are
  checked, and the lowering of each instance keeps the taken one. In the
  `then` branch the predicates **narrow** the type parameters they name:
  `meta.implements<T, I>()` adds the bound `I` to `T`;
  `meta.isInteger<T>()` adds `Copy`, `Eq`, `Ord`, `Hash`, `Clone`, `Debug`
  and `Display`, `meta.isFloat<T>()` the same without `Eq`, `Ord` and
  `Hash`; `meta.isStruct<T>()` and `meta.isEnum<T>()` allow
  `meta.fieldsOf<T>()` and `meta.variantsOf<T>()`. Narrowing goes through
  `&&` and not through `||` or `!`: under `A || B` a parameter is narrowed
  only by what both `A` and `B` give it.
- **`comptime for x in S`.** `S` is `keyof T` (spec 16.6),
  `meta.fieldsOf<T>()` or `meta.variantsOf<T>()`, with `T` a struct (or an
  enum) or narrowed to one (E0364). The loop variable stands for an
  implicit key parameter `K: keyof T`: a key in the first form, and a field
  descriptor `meta.Field<T, K>` in the second, with `field.name` (`str`),
  `field.index` (`int`), `field.attribute(key)` (`Option<str>`, from
  `@tag`) and `field.get(value)` (`value[K]`, of type `T[K]`). The body is
  checked once and lowered once per field of the instance, in declaration
  order, with `K` bound to that field's key. `break` and `continue` are not
  allowed in it.
- **`comptime panic(message)`** has type `never`. The message is a string
  literal whose interpolations are compile-time `str` values
  (`meta.nameOf<T>()`, `field.name`). Lowering an instance that keeps it
  reports E0814 at the call that created the instance, with the message.
- **`std.meta`** is a module of intrinsic functions (`std/meta.of`), like
  `alignOf`: the checker types them and the lowering replaces each call by
  a constant of the instance. `meta.nameOf<T>()` is the name `typeName`
  gives the concrete type.
- **`comptime` expressions and `comptime fn`** run in an interpreter over
  the AIR, after lowering and before code generation. A `comptime`
  expression's value is a number, `bool`, `str`, array, tuple, struct or
  enum of those (E0818 otherwise); its function may allocate, but nothing
  it allocates escapes. Calls to `extern` functions and to the runtime's
  I/O, clock, environment and random functions are E0815. A panic, an
  arithmetic or bounds error, or more than the step budget (10 million AIR
  statements, or `n` from `@comptimeBudget(n)` on the enclosing
  declaration) is E0816, with the panic message or the budget. A
  `comptime fn` called outside compile-time code is E0817.
- **Stage 1 only.** Stage 0 does not lex `comptime`, so the compiler and
  the parts of std that stage 0 compiles do not use it.

## Consequences

- Chapter 15 changes its wording: a `comptime for` body is checked once
  with a symbolic field type, and an untaken `comptime if` branch must
  still type-check under its narrowed bounds. `toJson` (spec 15.3) is
  written with `meta.isInteger<T>()` narrowing `T` to `Display`, and its
  recursive call `toJson(field.get(value), out)` instantiates `toJson` for
  `T[K]`, one instance per field type.
- Errors in compile-time code are reported once, at the definition, and do
  not depend on which instances a program uses. Code that type-checks only
  for some instances needs a narrowing predicate or `comptime panic`.
- The checker needs no per-instance state; the lowering gains a binding
  for the loop's key parameter while it lowers each copy of a `comptime
  for` body, which `concreteType` and the final substitution apply.
- The interpreter is a separate component that reads the AIR, so it runs
  the same code the program would, and can be built after the expansion.
- The work is split into steps: design (this ADR); `@tag`, `std.meta`
  predicates and `nameOf`, `comptime if` and `comptime panic`; `comptime
  for` over keys and fields; `variantsOf`; the interpreter, `comptime fn`
  and the budget.
