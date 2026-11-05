# Error code registry

Every diagnostic produced by an Ofir compiler has a stable code. Codes are
never reused or renumbered; a retired code is marked as such. Both compiler
stages use the same codes, so the `tests/compile_fail` suite works with
either.

Each code gets its own file, `EXXXX.md`, with a short explanation, an
example that triggers it and how to fix it. `ofir explain EXXXX` prints
that file.

## Ranges

| Range | Phase |
|-------|-------|
| `E0001`–`E0099` | lexer |
| `E0100`–`E0199` | parser |
| `E0200`–`E0299` | names, modules and visibility |
| `E0300`–`E0499` | types, methods, generics and interfaces |
| `E0500`–`E0599` | patterns and `match` |
| `E0600`–`E0699` | moves and initialization |
| `E0700`–`E0799` | borrows and reference escape |
| `E0800`–`E0899` | constants, attributes, FFI and `unsafe` |
| `E0900`–`E0999` | driver, files and toolchain |

Warnings use the same scheme with a `W` prefix (`W0001`, ...).

## Codes

| Code | Message | Since |
|------|---------|-------|
| E0001 | unexpected character | stage 0 |
| E0002 | unterminated string literal | stage 0 |
| E0003 | unterminated block comment | stage 0 |
| E0004 | invalid escape sequence | stage 0 |
| E0005 | integer literal too large | stage 0 |
| E0006 | unescaped `}` in string literal | stage 0 |
| E0007 | malformed multi-line string: text after the opening `"""`, or a line indented less than the closing `"""` | stage 1 |
| E0100 | expected X, found Y | stage 0 |
| E0200 | cannot find name `x` in this scope | stage 0 |
| E0201 | `x` is defined more than once | stage 0 |
| E0202 | `x` is private | stage 0 |
| E0203 | the program has no valid `main` function | stage 0 |
| E0204 | cannot find module `m` | stage 0 |
| E0205 | a `@test` function takes no parameters and returns nothing | stage 0 |
| E0300 | mismatched types: expected X, found Y | stage 0 |
| E0301 | no method `m` on type `T` | stage 0 |
| E0302 | wrong number of arguments | stage 0 |
| E0303 | function may reach its end without returning a value | stage 0 |
| E0304 | cannot assign twice to immutable variable `x` | stage 0 |
| E0305 | unused `Result` that must be used | stage 0 |
| E0306 | unknown, duplicate or misplaced named argument | stage 1 |
| E0307 | the `else` block of `let ... else` must not fall through | stage 0 |
| E0308 | missing value for a field or parameter without a default | stage 0 |
| E0309 | `break` or `continue` outside of a loop | stage 0 |
| E0310 | expected a value, found type `T` (or a type, found value `x`) | stage 0 |
| E0311 | expression of type `T` is not a function | stage 0 |
| E0312 | integer literal out of range for `T` | stage 0 |
| E0313 | operator cannot be applied to type `T` | stage 0 |
| E0314 | a value of type `T` cannot be interpolated into a string | stage 0 |
| E0315 | this value cannot be modified: declare it with `var` (or take it as `mut`) | stage 0 |
| E0316 | no field `f` on type `T` | stage 0 |
| E0317 | a `copy struct` can only have fields of Copy types | stage 0 |
| E0318 | a type cannot contain itself by value | stage 0 |
| E0319 | invalid or duplicate enum discriminant | stage 0 |
| E0320 | no variant `V` in enum `E` | stage 0 |
| E0321 | this operation needs a value of a Copy type | stage 0 |
| E0322 | type `T` cannot be cloned: a part of it has `drop` but no `clone` | stage 0 |
| E0326 | a variant is named through its enum: write `E.V` (ADR 0028) | stage 0 |
| E0330 | cannot infer the type parameter `T`: write the type arguments | stage 0 |
| E0331 | instantiating `f` nests types more than 64 levels deep | stage 0 |
| E0332 | the type `X` does not satisfy the bound `B` of `T` | stage 1 |
| E0333 | `B` is not an interface: only interfaces (and `Eq`, `Ord`, `Hash`, `Clone`) bound, embed or are asserted | stage 1 |
| E0334 | `X` is not a type parameter of this declaration: only type parameters take bounds | stage 1 |
| E0335 | the type `C` does not satisfy `I`, which it asserts (spec 9.2) | stage 1 |
| E0336 | cannot infer the type of the closure parameter `x`: write its type (spec 10.1) | stage 1 |
| E0337 | `x` is captured by a `move` closure: captured values are immutable (spec 10.3) | stage 1 |
| E0338 | `dyn I` is not allowed: a method of `I` without a body takes `self` by value, is generic or mentions `Self` (spec 9.7) | stage 1 |
| E0339 | method `m` cannot be called through `dyn I` (spec 9.7) | stage 1 |
| E0340 | `keyof` needs a struct or tuple type (spec 16) | planned |
| E0341 | `T` has no field `.k` (spec 16) | planned |
| E0342 | the fields of these keys have different types (spec 16) | planned |
| E0343 | a key must be known at compile time (spec 16) | planned |
| E0344 | the condition of a conditional type is not decided for this instantiation (spec 16) | planned |
| E0350 | the members `A` and `B` of a union can be the same type (spec 17) | planned |
| E0351 | type `T` cannot be a member of a union (spec 17) | planned |
| E0352 | `T` is not a member of the union `U` (spec 17) | planned |
| E0353 | the branches have different types: write the union type expected (spec 17) | planned |
| E0354 | a type test names a type that does not satisfy the interface of the `dyn` value (spec 9.8) | stage 1 |
| E0355 | `dyn I` has no size known at compile time: store a `Box<dyn I>` (spec 9.7) | stage 1 |
| E0356 | `I` is an interface: only a parameter type can name it alone; write `dyn I` or a type parameter (spec 9.6) | stage 1 |
| E0500 | non-exhaustive `match`: missing patterns | stage 0 |
| E0501 | refutable pattern in `let` | stage 0 |
| E0600 | use of moved value `x` | stage 0 |
| E0601 | use of possibly uninitialized variable `x` | stage 0 |
| E0602 | cannot move out of a borrowed or indexed place | stage 0 |
| E0700 | cannot borrow `x` as mutable more than once | stage 1 |
| E0701 | cannot borrow `x` as mutable because it is also borrowed as shared | stage 1 |
| E0702 | a reference or a borrowed function value cannot be stored or returned here | stage 0 (references), stage 1 (function values) |
| E0703 | cannot use `x` while it is borrowed as mutable | stage 1 |
| E0704 | cannot assign to `x` while it is borrowed | stage 1 |
| E0705 | cannot move out of `x` while it is borrowed | stage 1 |
| E0706 | `x` does not live long enough: dropped at the end of its block while borrowed | stage 1 |
| E0800 | the constant `x` depends on itself | stage 0 |
| E0801 | a constant may only use literals, constants and operators | stage 0 |
| E0802 | integer overflow, division by zero, invalid shift or cast in a constant | stage 0 |
| E0803 | this operation requires an `unsafe` block or function | stage 0 |
| E0804 | type `T` has no C layout and cannot cross the C boundary | stage 0 |
| E0805 | invalid `@lang` declaration (reserved to the standard library) | stage 0 |
| E0806 | a `@lang` declaration does not have the layout of its primitive type | stage 0 |
| E0900 | cannot read source file | stage 0 |
| E0901 | X is not supported yet (a feature this compiler version does not implement) | stage 0 |
| E0902 | the toolchain failed: clang or the runtime library is missing, or clang reported an error | stage 0 |

### Warnings

| Code | Message | Since |
|------|---------|-------|
| W0001 | unused variable `x` | stage 0 |
| W0002 | unused import | stage 0 |
| W0003 | unused private item | stage 1 |
| W0004 | unreachable code | stage 0 |
| W0005 | variable does not need to be `var` | stage 1 |
| W0006 | use of deprecated item | stage 1 |
| W0007 | name does not follow the naming convention | stage 1 |

New codes are added to these tables in the same commit as the diagnostic.
