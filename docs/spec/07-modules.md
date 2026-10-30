# 7. Modules

## 7.1 Files and modules

- Every `.astra` file is a module. The module name is the file name.
- A directory groups modules into a package: `src/parser/lexer.astra` is the
  module `parser.lexer`.
- A program is built from a root file containing `fn main()`.

## 7.2 Imports

```astra
import std.io
import std.collections.Map
import parser.lexer
import parser.ast as ast
```

- Imported modules are accessed by their last name segment:
  `lexer.tokenize(src)`, or by the alias given with `as`.
- Importing a type (`import std.collections.Map`) brings that name into scope.
- Imports are resolved relative to the project root (the directory of the
  root file) and to the standard library.
- Circular imports are allowed; declarations in a module can reference each
  other in any order.
- Unused imports are a compile error.

## 7.3 Visibility

Declarations are private to their module by default. `pub` makes them
visible to importers:

```astra
pub struct Token {
    pub kind: TokenKind
    pub text: str
    line: int          // private field
}

pub fn tokenize(src: &str) -> List<Token> { ... }
```

## 7.4 The prelude

The following names are available in every module without imports:

- primitive types, `str`, `List`, `Map`, `Option`, `Result`, `Box`
- `Some`, `None`, `Ok`, `Err`
- `print`, `println`, `eprintln`, `panic`, `assert`

## 7.5 Standard library (bootstrap subset)

| Module | Contents |
|--------|----------|
| `std.io` | stdin/stdout/stderr, buffered readers and writers |
| `std.fs` | read and write files, directories |
| `std.os` | command line arguments, environment, exit, run processes |
| `std.str` | string helpers: split, trim, find, parse numbers |
| `std.math` | `sqrt`, `pow`, `floor`, trigonometric functions |

These are the modules the self-hosted compiler needs. The rest of the
standard library is **[planned]**.
