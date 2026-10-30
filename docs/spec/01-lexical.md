# 1. Lexical structure

## 1.1 Source files

Source files are UTF-8 encoded text with the extension `.at`.

## 1.2 Comments

```astra
// line comment
/* block comment */
```

Block comments do not nest.

## 1.3 Statement termination

Astra has no semicolons. The rule is:

> A newline ends a statement **wherever the statement could end**.
> Anywhere else, the newline is ignored and the statement continues on the
> next line.

The lexer emits a newline token at the end of each line; the parser treats
it as a terminator only where the grammar allows the current statement to
end, and skips it otherwise. (Unlike Go's token-based rule, this works with
`<...>` generics: `var buffer: List<u8>` at the end of a line is complete,
while `a >` at the end of a line clearly continues.)

```astra
var buffer: List<u8>          // complete: the newline ends the statement
let ok = width > 0 &&
    height > 0                // `&&` needs an operand: the newline is ignored
let total = price
    + tax                     // error: `price` was complete, `+ tax` is a new statement
```

To split a long expression, end the line with the operator, a `,`, or an
opening bracket.

Newlines inside `(...)` and `[...]` are always ignored, so long argument
lists and array literals can span several lines:

```astra
let total = add(
    first,
    second,
)
```

A consequence: an opening `{` must be on the same line as the construct it
belongs to, and `else` must follow `}` on the same line.

## 1.4 Identifiers

```ebnf
identifier = letter { letter | digit } .
letter     = "a"…"z" | "A"…"Z" | "_" .
digit      = "0"…"9" .
```

Conventions (enforced by the formatter, not the compiler):
`snake_case` for functions, variables and modules; `PascalCase` for types and
enum variants; `SCREAMING_CASE` for constants.

## 1.5 Keywords

```
as        break     const     continue  copy      dyn       else
enum      extern    false     fn        for       if        import
in        interface let       match     move      mut       pub
ref       return    Self      struct    true      type      unsafe
var       where
```

## 1.6 Operators and punctuation

```
+    -    *    /    %    &    |    ^    <<   >>   !
+=   -=   *=   /=   %=   &=   |=   ^=   <<=  >>=
==   !=   <    <=   >    >=   &&   ||
=    .    ..   ..=  ,    :    =>   ?    @
(    )    [    ]    {    }    ;
```

`;` is used only inside array types and repeat literals (`[u8; 16]`,
`[0; 16]`); it never terminates statements.

## 1.7 Literals

### Integers

```astra
42
1_000_000
0xff
0o755
0b1010_0101
```

An integer literal has no fixed type; it takes the type required by its
context and defaults to `int`. A literal that does not fit its type is a
compile error.

### Floats

```astra
3.14
1e9
2.5e-3
```

A float literal defaults to `float` (64 bits). An integer literal may be used
where a float is expected (`let x: float = 1`).

### Strings

```astra
"hello\n"
"name = {user.name}, age = {user.age + 1}"
```

Escape sequences: `\n`, `\t`, `\r`, `\0`, `\\`, `\"`, `\{`, `\}`,
`\xHH` (a byte) and `\u{HHHHHH}` (a Unicode scalar value).

**Interpolation:** `{expr}` inside a string literal evaluates `expr` and
inserts its textual form. The type of `expr` must implement string
conversion (all primitive types do). A literal brace is written `\{` or `\}`.

**Format specifiers:** `{expr:spec}` controls formatting:

| Spec | Meaning | Example |
|------|---------|---------|
| `?` | debug representation (`Debug`) | `{list:?}` → `[1, 2, 3]` |
| `.N` | N digits after the decimal point | `{pi:.2}` → `3.14` |
| `N` | minimum width, right aligned | `{n:5}` → `   42` |
| `<N` | minimum width, left aligned | `{s:<8}` |
| `0N` | zero padded | `{n:04}` → `0042` |
| `x` / `X` / `b` | hexadecimal / binary | `{n:x}` → `ff` |

### Raw strings

Raw strings start with `r"` (single line) or `"""` (multi-line). They have no
escapes and no interpolation. In multi-line strings, the indentation of the
closing `"""` is removed from every line.

```astra
let pattern = r"C:\path\{name}"
let text = """
    first line
    second line
    """
```

### Characters

There is no `char` type. A byte literal `b'a'` has type `u8`.

## 1.8 Documentation comments

`///` documents the next declaration and `//!` documents the enclosing
module (chapter 12.5).
