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

Newlines directly inside `(...)` and `[...]` are always ignored, so long
argument lists and array literals can span several lines:

```astra
let total = add(
    first,
    second,
)
```

"Directly" means that the innermost open bracket is `(` or `[`. Inside a
`{ }` block the newline rule applies again, even when the block itself is
inside parentheses, so a closure passed as an argument has normal
statements:

```astra
thread.spawn(move () => {
    let x = compute()       // the newline ends this statement
    send(x)
})
```

A consequence: an opening `{` must be on the same line as the construct it
belongs to, and `else` must follow `}` on the same line.

## 1.4 Identifiers

```ebnf
identifier = letter { letter | digit } .
letter     = "a"…"z" | "A"…"Z" | "_" .
digit      = "0"…"9" .
```

### Naming conventions

| Kind | Convention | Example |
|------|------------|---------|
| functions, methods, variables, parameters, fields | `camelCase` | `readToString`, `lineCount` |
| types, interfaces, enum variants, type parameters | `PascalCase` | `TokenKind`, `IoError`, `.NotFound`, `T` |
| constants | `SCREAMING_CASE` | `MAX_USERS` |
| modules and file names | lowercase `snake_case` | `std.collections`, `word_count.at` |

Acronyms are written as words: `parseUrl`, `HttpClient`, `IoError` (not
`parseURL`, `HTTPClient`). Module and file names stay lowercase so that they
behave the same on case-insensitive file systems.

The conventions are checked by a warning (chapter 12.6), not by the grammar.
Names declared in `extern "C"` blocks are exempt, since they must match C.

## 1.5 Keywords

Reserved keywords cannot be used as identifiers:

```
as        break     const     continue  copy      dyn       else
enum      extern    false     fn        for       if        import
in        interface let       match     mut       pub       return
self      Self      struct    true      unsafe    var
```

Contextual keywords are keywords only in one position and remain valid
identifiers everywhere else, because they are common variable and field
names:

| Word | Keyword only when |
|------|-------------------|
| `type` | it starts an item and is followed by an identifier (`type Id = u32`) |
| `move` | it is followed by `fn`, or starts a parameter mode (`x: move T`) |
| `ref` | it is followed by `struct` (`ref struct Iter { }`) |
| `where` | it follows a function signature |
| `comptime` | it starts a declaration, block, `for`, `if` or expression (chapter 15) |

```astra
let type = token.type      // fine: `type` is an ordinary identifier here
```

## 1.6 Operators and punctuation

```
+    -    *    /    %    &    |    ^    <<   >>   !
+=   -=   *=   /=   %=   &=   |=   ^=   <<=  >>=
==   !=   <    <=   >    >=   &&   ||
=    .    ..   ..=  ,    :    =>   ?    ??   @
(    )    [    ]    {    }    ;
```

The lexer never combines `>` with a following character: `>>`, `>=` and
`>>=` are recognized by the parser from adjacent `>` tokens with no space
between them. This lets `List<List<int>>` close two type argument lists.

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
`\xHH` (an ASCII character, `\x00` to `\x7F`) and `\u{HHHHHH}` (a Unicode
scalar value). Escapes cannot produce invalid UTF-8, so every `str` is valid
UTF-8. Arbitrary bytes belong in byte strings, `b"\xff\x00"`, of type
`&[u8]`.

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

### Characters and bytes

There is no `char` type. A byte literal `b'a'` has type `u8`; a byte string
literal `b"GET \x00"` has type `&[u8]` and accepts any `\xHH` escape.
Unicode scalar values are `u32` (see `str.chars()` in the standard library).

## 1.8 Documentation comments

`/** ... */` documents the next declaration; a `/** ... */` comment whose
text starts with `@module` documents the file's module (chapter 12.5, ADR
0020). The leading `*` of each line is not part of the text. `/***` and
`/**/` are ordinary block comments.
