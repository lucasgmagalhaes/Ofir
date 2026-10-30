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

fn log(message: str) {          // returns ()
    println(message)
}
```

```ebnf
FnDecl = [ "pub" ] "fn" identifier "(" [ Params ] ")" [ Type ] Block .
Params = Param { "," Param } [ "," ] .
Param  = identifier ":" Type .
```

- The return type follows the parameter list directly, as in Go:
  `fn name() str`. A function with no return type returns `()`.
- Parameters are borrowed by default; `mut` and `move` change how they are
  passed (chapter 5.3). The caller never marks the mode:

  ```astra
  fn rename(user: mut User, name: move str) { user.name = name }

  rename(ana, "Ana Maria")
  ```
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

Methods are declared **inside** the body of the struct or enum they belong
to, after its fields or variants. A method takes `self` as its first
parameter; the mode of `self` (chapter 5.3) says how the value is used:

```astra
pub struct Point {
    pub x: float
    pub y: float

    pub fn origin() Point {                 // no `self`: associated function
        return Point{x: 0.0, y: 0.0}
    }

    pub fn length(self) float {             // reads the point
        return (self.x * self.x + self.y * self.y).sqrt()
    }

    pub fn scale(mut self, k: float) {      // modifies the point
        self.x *= k
        self.y *= k
    }

    pub fn intoVec2(move self) Vec2 {       // consumes the point
        return Vec2{x: self.x, y: self.y}
    }
}
```

```astra
var p = Point.origin()
p.scale(2.0)
let l = p.length()
```

- Inside a method, fields and other methods are always reached through
  `self` (`self.x`, `self.length()`); there is no implicit `this`.
  Associated functions are called through the type name, or `Self` inside
  the type (`Self.origin()`).
- `self` has the type of the enclosing struct or enum, `Self`.
- `pub` on a method makes it visible to other modules; methods are private
  by default, like everything else.
- The type parameters of a generic type are in scope in all its methods
  (chapter 8.3).
- All the methods of a type are declared in its body, so they always live in
  the type's module.
- By convention fields come first, then associated functions, then methods;
  the formatter enforces this order.

Enums declare methods after their variants:

```astra
enum Shape {
    Circle(radius: float)
    Rect(width: float, height: float)

    fn area(self) float {
        return match self {
            .Circle(r) => 3.14159 * r * r
            .Rect(w, h) => w * h
        }
    }
}
```

## 3.7 Destructors

A type may define a `drop` method, called automatically when a value of that
type goes out of scope (chapter 5):

```astra
struct File {
    fd: i32

    fn drop(mut self) {
        close(self.fd)
    }
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
