# Error code registry

Every diagnostic produced by an Astra compiler has a stable code. Codes are
never reused or renumbered; a retired code is marked as such. Both compiler
stages use the same codes, so the `tests/compile_fail` suite works with
either.

Each code gets its own file, `EXXXX.md`, with a short explanation, an
example that triggers it and how to fix it. `astra explain EXXXX` prints
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
| E0100 | expected X, found Y | stage 0 |
| E0200 | cannot find name `x` in this scope | stage 0 |
| E0201 | `x` is defined more than once | stage 0 |
| E0202 | `x` is private | stage 0 |
| E0203 | the program has no valid `main` function | stage 0 |
| E0204 | cannot find module `m` | stage 0 |
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
| E0320 | no variant `V` in enum `E`, or the enum of `.V` is unknown | stage 0 |
| E0500 | non-exhaustive `match`: missing patterns | stage 0 |
| E0501 | refutable pattern in `let` | stage 0 |
| E0600 | use of moved value `x` | stage 0 |
| E0601 | use of possibly uninitialized variable `x` | stage 0 |
| E0602 | cannot move out of a borrowed or indexed place | stage 0 |
| E0700 | cannot borrow `x` as mutable more than once | stage 1 |
| E0701 | cannot borrow `x` as mutable because it is also borrowed as shared | stage 1 |
| E0702 | a reference cannot be stored or returned here | stage 1 |
| E0800 | the constant `x` depends on itself | stage 0 |
| E0801 | a constant may only use literals, constants and operators | stage 0 |
| E0802 | integer overflow, division by zero, invalid shift or cast in a constant | stage 0 |
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
