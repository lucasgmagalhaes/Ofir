# 0029. An interface as a parameter type is an anonymous type parameter

- Status: Accepted
- Date: 2026-11-05

## Context

An interface is used in two ways (ADR 0004): as a bound, with static
dispatch, and behind `dyn`, with dynamic dispatch. The static form, the
one that costs nothing and the one most functions want, is the more
verbose of the two:

```ofir
fn draw<S: Shape>(s: S) { ... }    // static: a type parameter for one use
fn draw(s: dyn Shape) { ... }      // dynamic
```

Writing an interface name alone, `fn draw(s: Shape)`, is an error today
(E0310). Three meanings were considered for it:

1. **An error, as today.** Explicit, but the common case pays a type
   parameter that is named only to be used once.
2. **`dyn Shape`, as in Go**, where every interface value is dynamic. It
   hides the cost of a `dyn` value (a fat pointer, an indirect call, no
   inlining), which goes against the rule that the reader can predict what
   the code does (spec 9.9), and it makes the type of a parameter look
   like a struct when it is not one: `let x: Shape` could still not exist,
   since `dyn Shape` has no size. Rust made this form an error (edition
   2021) for these reasons.
3. **An anonymous type parameter**, as Rust's `impl Trait` and Swift's
   `some Protocol` in argument position: `fn draw(s: Shape)` means
   `fn draw<S: Shape>(s: S)`. The short form is the cheap one, and `dyn`
   stays where dynamic dispatch is needed: values of different types in
   one collection, and code size over speed (ADR 0007).

## Decision

An interface named as the type, or as part of the type, of a function or
method parameter declares an **anonymous type parameter** bound by that
interface:

```ofir
fn draw(s: Shape) { ... }
// is
fn draw<S: Shape>(s: S) { ... }

fn totalArea(shapes: [Shape]) float { ... }
// is
fn totalArea<S: Shape>(shapes: [S]) float { ... }
```

- **Where.** In the type of a parameter of a function or a method,
  anywhere in it: `s: Shape`, `s: mut Shape`, `s: move Shape`,
  `shapes: [Shape]`, `r: &Shape`, `items: List<Shape>`,
  `found: Option<Shape>`. Not in a return type, a field, a `let`, a type
  argument written at a call, a `fn(...)` type or the parameter of a
  closure; there an interface name is an error (E0356), whose message
  suggests `dyn I` or a type parameter.
- **One parameter per occurrence.** Each occurrence is a type parameter of
  its own: `fn same(a: Shape, b: Shape)` accepts a `Circle` and a
  `Square`. Values that must have the same type use a named type
  parameter: `fn same<S: Shape>(a: S, b: S)`.
- **Order and type arguments.** The anonymous parameters come after the
  named ones and are always inferred: type arguments written at a call,
  `f<int>(...)`, give only the named ones. There is no way to name an
  anonymous parameter; code that needs to (a second use of the type, a
  `where` clause, a return type) declares it.
- **Several interfaces.** One interface name per position; a parameter
  bound by several interfaces is declared, `<S: Shape + Display>`.
- **Same as a named parameter.** Everything else is the rule of a
  generic function (spec 8): the function is monomorphized for each
  concrete type (ADR 0007), the body sees only the methods of the
  interface, and an interface method that has such a parameter is generic,
  so it is not callable through `dyn` (spec 9.7, E0338).
- **`dyn` is unchanged.** `s: dyn Shape` is still the dynamic form, and
  still the only one that stores values of different types together:
  `List<Box<dyn Shape>>`. `items: List<Shape>` is a list of one type.

```ofir
fn printAll(shapes: [Shape]) {          // one Shape type per call: static
    for s in shapes {
        println(s.describe())
    }
}

fn printAny(shapes: [Box<dyn Shape>]) { // any Shape types: dynamic
    for s in shapes {
        println(s.describe())
    }
}
```

## Consequences

- The common case is the short one, and the short one costs nothing; the
  extra cost of dynamic dispatch is still written (`dyn`).
- No program changes meaning: the form was an error (E0310).
- An interface name now means different things by position: a bound
  after `:` in `<S: Shape>`, a type parameter in a parameter type, an
  error elsewhere. The rule is one sentence (only parameter types), and
  E0356 explains the other positions.
- The type of an anonymous parameter cannot be named, so a function whose
  return type depends on it (`fn biggest(a: Shape, b: Shape) S`) needs the
  named form. A return type that hides the concrete type (`impl Trait` in
  return position) is a separate decision, not taken here.
- Generic interfaces (spec 9.5) work the same way once supported:
  `it: Iterator<int>` is `<I: Iterator<int>>(it: I)`.
- The compiler desugars the form while resolving names, before type
  checking, so the checker, the monomorphizer and the code generator see
  ordinary type parameters. Messages show the parameter as written,
  `Shape`, not an invented name.
- It is implemented by roadmap item 5.4c.
