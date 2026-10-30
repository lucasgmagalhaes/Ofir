# 7. Modules

## 7.1 Files and modules

- Every `.at` file is a module. The module name is the file name.
- A directory groups modules into a package: `src/parser/lexer.at` is the
  module `parser.lexer`. A file named `mod.at` inside a directory is the
  module named after the directory itself (`src/parser/mod.at` is `parser`).
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
- An unused import produces a warning (chapter 12.6).

## 7.3 Visibility

Declarations are private to their module by default. `pub` makes them
visible to importers:

```astra
pub struct Token {
    pub kind: TokenKind
    pub text: str
    line: int          // private field
}

pub fn tokenize(src: &str) List<Token> { ... }
```

## 7.4 The prelude

The following names are available in every module without imports:

- primitive types, `str`, `List`, `Map`, `Option`, `Result`, `Box`
- `Some`, `None`, `Ok`, `Err`
- the built-in functions below

## 7.5 Built-in functions

```astra
fn print(text: &str)              // standard output
fn println(text: &str)            // standard output, followed by "\n"
fn eprint(text: &str)             // standard error
fn eprintln(text: &str)           // standard error, followed by "\n"
fn panic(message: &str) !
fn assert(condition: bool)
fn assert_eq<T: Eq + Debug>(left: &T, right: &T)
fn size_of<T>() int
fn align_of<T>() int
```

Values are printed through string interpolation: `println("{count}")`.
There are no variadic functions in Astra.

## 7.6 Standard library (bootstrap subset)

| Module | Contents |
|--------|----------|
| `std.io` | stdin/stdout/stderr, buffered readers and writers |
| `std.fs` | read and write files, directories |
| `std.os` | command line arguments, environment, exit, run processes |
| `std.strings` | string helpers: split, trim, find, parse numbers (named so it does not shadow the `str` type) |
| `std.math` | constants and functions such as `math.PI`, `math.hypot`; float methods like `x.sqrt()` live on the type |
| `std.mem` | `swap`, `replace`, `take`, `UnsafeCell`, `Cell` |

These are the modules the self-hosted compiler needs. The rest of the
standard library is **[planned]**.
