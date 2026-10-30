# 3. Declarations

## 3.1 Variables

```astra
let name = "Astra"        // immutable binding
var count = 0             // mutable binding
var total: float = 0      // explicit type
var buffer: List<u8>      // must be assigned before its first use
```

- `let` bindings cannot be reassigned, and the value they own cannot be
  mutated through them.
- `var` bindings can be reassigned and mutated.
- A variable must be definitely initialized before it is read. A declaration
  without a value (`var x: int`) is allowed only if every path assigns it
  before its first use. There are no implicit zero values.
- A name may shadow a name from an enclosing scope, but not one declared in
  the same scope.

## 3.2 Constants

```astra
const MAX_USERS = 1024
const GREETING: str = "hello"
```

Constants are evaluated at compile time. Their initializer may only use
literals, other constants and arithmetic operators.

## 3.3 Functions

```astra
fn add(a: int, b: int) int {
    return a + b
}

fn log(message: &str) {          // returns ()
    println(message)
}
```

```ebnf
FnDecl = [ "pub" ] "fn" [ Receiver ] identifier "(" [ Params ] ")" [ Type ] Block .
Params = Param { "," Param } [ "," ] .
Param  = identifier ":" Type .
```

- The return type follows the parameter list directly, as in Go:
  `fn name() str`. A function with no return type returns `()`.
- Parameters are immutable bindings. To mutate a parameter locally, rebind
  it: `var n = n`.
- A function with a non-`()` return type must `return` on every path; the
  compiler rejects functions where control can reach the end of the body.
- The order of declarations in a file does not matter.

### Entry point

Every program has exactly one `fn main()` in its root module. It takes no
parameters and returns `()` or `Result<(), E>` where `E` satisfies
`Display`. If `main` returns an `Err`, the error is printed to standard error
as `error: <message>` and the process exits with status 1. Command line
arguments are available through `std.os.args()`.

## 3.4 Structs

```astra
struct User {
    name: str
    email: str
    age: int
}

let u = User{name: "Ana", email: "ana@example.com", age: 30}
let v = User{age: 31, ..u}     // copy/move the remaining fields from u
```

- All fields must be initialized in a struct literal.
- `pub` on the struct makes the type visible to other modules; `pub` on a
  field makes the field visible. Fields are private to the module by default.
- A struct may not contain itself by value, directly or indirectly; use
  `Box<T>` or a collection instead.

## 3.5 Enums

```astra
enum Token {
    Number(value: int)
    Ident(name: str)
    Plus
    Minus
}

let t = Token.Number(42)
let p = Token.Plus
```

When the expected type is known, the enum name can be omitted:
`let t: Token = .Plus`.

## 3.6 Methods

A method is a function with a receiver, written before its name as in Go:

```astra
fn (p: &Point) length() float { ... }        // reads p
fn (p: &mut Point) scale(k: float) { ... }      // modifies p
fn (p: Point) intoVec2() Vec2 { ... }      // consumes p
fn Point.origin() Point { ... }              // associated function (no receiver)
```

The receiver form defines how the value is passed (see chapter 5). At a call
site, the compiler borrows automatically:

```astra
var p = Point.origin()
p.scale(2.0)       // same as Point.scale(&mut p, 2.0)
let l = p.length() // same as Point.length(&p)
```

Methods can only be declared in the module that declares the type.

## 3.7 Destructors

A type may define a `drop` method, called automatically when a value of that
type goes out of scope (chapter 5):

```astra
fn (f: &mut File) drop() {
    close(f.fd)
}
```

## 3.8 External functions

Functions implemented in C (libc or the Astra runtime) are declared in an
`extern "C"` block and can only be called from `unsafe` code:

```astra
extern "C" {
    fn write(fd: i32, buf: *u8, len: uint) int
}
```

See chapter 11 for the full rules.
