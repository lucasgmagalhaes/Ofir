# Compiler architecture

This document describes how the Astra compiler is organized internally. It
applies to both the stage 0 (C) and stage 1 (Astra) compilers; stage 0
implements a subset of each phase.

## 1. Pipeline

```
                ┌──────────┐   ┌────────┐   ┌──────────┐   ┌────────────┐
    .at files ─►│  lexer   │──►│ parser │──►│ resolver │──►│type checker│
                └──────────┘   └────────┘   └──────────┘   └─────┬──────┘
                  tokens          AST        symbols, scopes      │ typed AST
                                                                  ▼
 ┌────────┐   ┌──────────┐   ┌─────────┐   ┌──────────┐   ┌────────────┐
 │ clang  │◄──│ codegen  │◄──│  mono   │◄──│  drops   │◄──│  lowering  │
 └───┬────┘   └──────────┘   └─────────┘   └────┬─────┘   └─────┬──────┘
     │          LLVM IR       concrete AIR       │ AIR + drops    │ AIR
     ▼                                      ┌────┴─────┐          │
 executable                                 │  borrow  │◄─────────┘
                                            │  checker │
                                            └──────────┘
```

| Phase | Input | Output | Errors reported |
|-------|-------|--------|-----------------|
| Lexer | bytes | tokens | invalid characters, unterminated literals |
| Parser | tokens | AST | syntax errors |
| Resolver | AST | symbol table, resolved names | unknown names, duplicate definitions, import cycles in constants |
| Type checker | resolved AST | types for every node | type mismatches, missing methods, unsatisfied bounds, non-exhaustive matches |
| Lowering | typed AST | AIR (generic) | — |
| Borrow checker | AIR | — | use after move, conflicting borrows, escaping references |
| Drop elaboration | AIR | AIR with explicit drops | — |
| Monomorphization | generic AIR | concrete AIR | recursion limit |
| Codegen | concrete AIR | LLVM IR text | — |
| clang | LLVM IR + runtime | executable | (internal errors only) |

Phases after type checking never report user errors except the borrow
checker. If any phase reports an error, compilation stops after that
phase (the phase itself continues to collect as many errors as possible).

## 2. Core data structures

### Identifiers instead of pointers

All compiler entities are stored in arenas and referenced by typed integer
indices. This makes data structures compact, cache friendly, trivially
serializable, and deterministic.

| Index | Refers to |
|-------|-----------|
| `FileId` | a source file in the file table |
| `NodeId` | an AST node |
| `DefId` | a declaration (function, struct, enum, interface, const, ...) |
| `TypeId` | an interned type |
| `LocalId` | a local variable inside one AIR body |
| `BlockId` | a basic block inside one AIR body |
| `Symbol` | an interned identifier string |

### Spans

Every token, AST node and AIR statement carries a `Span { file, start, end }`
with byte offsets. Line and column are computed only when rendering a
diagnostic.

### Type interning

Types are hash-consed: each distinct type exists exactly once and is
identified by its `TypeId`. Type equality is integer comparison.

```
TypeKind = Int(width, signed) | Float(width) | Bool | Unit | Never | Str
         | Adt(DefId, args: [TypeId])        // struct, enum
         | Ref(mutable, TypeId) | Ptr(mutable, TypeId)
         | Slice(TypeId) | Array(TypeId, len) | Tuple([TypeId])
         | Fn([TypeId], TypeId) | Dyn([DefId]) | Param(index)
         | Infer(var) | Error
```

`Error` is compatible with every type; it is produced after a reported
error so that one mistake does not cause a cascade of follow-up errors.

### Memory

The compiler allocates almost everything from arenas that live for the whole
compilation and are freed at once at exit. The compiler itself therefore
needs no fine-grained memory management, which keeps stage 0 simple and fast.

## 3. Lexer

- Hand written, single pass over bytes, no backtracking.
- Inserts statement terminators (`NL`) following chapter 1.3 of the spec.
- String interpolation is split into tokens:
  `"a {x} b"` → `StrStart("a ")`, tokens of `x`, `StrMid(" b")`... `StrEnd`.
  The lexer keeps a small stack of brace depths to know when `}` closes an
  interpolation.
- Doc comments are kept as tokens attached to the next item; ordinary
  comments are dropped (the formatter uses a separate trivia-preserving mode).

## 4. Parser

- Recursive descent for items and statements, Pratt (precedence climbing)
  for expressions, matching the precedence table of spec chapter 4.8.
- Struct-literal restriction (`if x == P{...}`) is handled with a
  "no struct literal" flag, as in Go.
- Generic disambiguation (`f<T>(x)` vs comparisons) uses bounded
  speculative parsing as specified in spec 8.5.
- **Error recovery:** on an unexpected token, the parser reports one error
  and skips to the next statement terminator or closing brace at the same
  nesting level, then continues. Each file yields as many independent
  syntax errors as possible.
- The AST is a faithful representation of the source; no desugaring happens
  in the parser.

## 5. Resolver

Two passes over each module:

1. **Collect:** register every top-level declaration in the module scope and
   record imports. Load imported modules (each file is parsed once).
2. **Resolve:** walk function bodies, binding each identifier to a `DefId`,
   a local, a type parameter or a module.

Scopes form a chain: block → function → module → prelude. Methods are
collected per type in a `MethodTable` keyed by `(DefId of type, Symbol)`.

## 6. Type checker

### Strategy

**Bidirectional type checking with local inference**: each expression is
checked against an *expected type* when one is known, otherwise its type is
*synthesized*. Inference variables are used only inside a function body, for:

- integer and float literals (defaulting to `int` / `float`);
- type arguments of generic calls (`max(a, b)`);
- closure parameters;
- empty collection literals (`[]`).

Unification is first-order; signatures are always explicit, so inference
never crosses function boundaries. This keeps type checking linear in
practice and error messages local.

### Method resolution

For `recv.m(args)`:

1. Look up `m` in the method table of the receiver's type, auto-dereferencing
   through references (`&&T` → `&T` → `T`).
2. Then in the default methods of interfaces that the type satisfies.
3. Adjust the receiver to the method's receiver form (`&`, `&mut` or move),
   recording the adjustment for lowering.

### Interfaces

- Satisfaction `T: I` is computed structurally and cached per `(TypeId, DefId)`.
- For generic code, a bound `T: I` makes `I`'s methods callable on `T`.
- For `dyn I`, the checker records the `(concrete type, interface)` pairs
  that need a vtable; codegen emits one constant vtable per pair.

### Patterns and exhaustiveness

Patterns are type checked against the scrutinee type. Exhaustiveness and
unreachable-arm detection use the *usefulness* algorithm (Maranget,
"Warnings for pattern matching"), which also produces the list of missing
patterns for the error message.

## 7. AIR — Astra Intermediate Representation

AIR is a control-flow graph of basic blocks per function, similar in spirit
to Rust's MIR. It is the input to borrow checking, drop elaboration,
monomorphization and code generation.

```
Body       = { locals: [Local], blocks: [Block], params: [LocalId] }
Local      = { type: TypeId, name: Symbol?, mutable: bool, span }
Block      = { statements: [Statement], terminator: Terminator }

Statement  = Assign(Place, Rvalue)
           | StorageLive(LocalId) | StorageDead(LocalId)
           | Drop(Place)                 (inserted by drop elaboration)
           | SetDropFlag(LocalId, bool)

Place      = LocalId + projections: [Field(i) | Deref | Index(LocalId) | Downcast(variant)]

Rvalue     = Use(Operand) | BinaryOp(op, Operand, Operand) | CheckedBinaryOp(...)
           | UnaryOp(op, Operand) | Ref(mutable, Place) | Aggregate(kind, [Operand])
           | Cast(kind, Operand, TypeId) | Discriminant(Place) | Len(Place)

Operand    = Copy(Place) | Move(Place) | Constant(value)

Terminator = Goto(BlockId)
           | Switch(Operand, [(value, BlockId)], otherwise: BlockId)
           | Call(callee, [Operand], destination: Place, next: BlockId)
           | Assert(Operand, expected: bool, PanicKind, next: BlockId)
           | Return | Unreachable
```

Properties:

- Every expression with side effects is broken down into statements; all
  operands are places or constants.
- Control flow (`if`, `match`, `for`, `?`, `&&`, `||`) becomes explicit
  blocks and switches.
- Runtime checks (overflow, bounds, division by zero) are explicit `Assert`
  terminators, so later passes can remove the ones proven redundant.
- AIR is validated after each pass in debug builds (`air/validate`).

### Lowering highlights

| Source construct | AIR |
|------------------|-----|
| `a + b` (integers) | `CheckedBinaryOp` + `Assert(no overflow)` |
| `xs[i]` | `Len` + `Assert(i < len)` + `Index` projection |
| `x?` | `Discriminant` + `Switch`; the error arm assigns the return place and jumps to the return block |
| `match` | decision tree of `Switch` on discriminants and literals |
| `for x in list` | index loop with `Len`, `Index` and a borrowed element |
| `for x in iter` | loop calling `next()` and switching on `Option` |
| string interpolation | calls to `Display.format` on a fresh `str` buffer |
| closures | an environment struct + a function taking it as first parameter |
| method call | plain call with the adjusted receiver as first argument |

## 8. Borrow checker

Runs on generic AIR, once per function, using dataflow analysis over the CFG:

1. **Initialization and moves** — forward "maybe initialized / maybe moved"
   analysis per place. Reading a place that may be uninitialized or moved is
   an error.
2. **Borrows** — each `Ref` creates a loan. A loan is live from its creation
   to the last use of the reference (liveness analysis). A write, move or
   conflicting borrow of a place overlapping a live loan is an error.
3. **Escape rules** — checks the second-class reference rules: references and
   `ref struct` values may not be stored into non-ref aggregates or returned,
   except through the single-reference-parameter rule (spec 5.6).

Because references cannot escape, every loan is local to one function body:
no inter-procedural analysis and no lifetime inference are needed.

## 9. Drop elaboration

Using the move analysis results, inserts `Drop(place)` at:

- scope exits (`StorageDead`) of initialized locals with drop glue;
- assignments that overwrite an initialized value;
- unwinding of `?` and early `return`.

Where a value is moved on some paths only, a boolean **drop flag** local is
introduced and the drop becomes conditional. LLVM removes most of these flags
after optimization.

**Drop glue** for a type is generated once: call the user `drop` method if
present, then drop each field (and, for enums, the active variant's fields).

## 10. Monomorphization

Starting from `main` (or all `@test` functions), a worklist collects every
function instance `(DefId, [TypeId])` reachable through calls, vtables and
function values. Each instance gets a concrete copy of the AIR body with
type parameters substituted. Instances are deduplicated by their key.
Drop glue and vtables are instantiated in the same pass.

Symbol names are mangled deterministically:
`_A` + module path + item name + encoded type arguments.

## 11. Code generation

AIR is translated to **textual LLVM IR** (`.ll`), see ADR 0001.

- Each AIR local becomes an `alloca` in the entry block; LLVM's `mem2reg`
  and `SROA` passes promote them to registers.
- Integer overflow checks use `llvm.sadd.with.overflow.*` and friends;
  the panic branch calls a `cold noreturn` runtime function.
- `&mut T` parameters are emitted with `noalias`; `&T` parameters with
  `noalias readonly` when the type has no interior mutability.
- Internal functions use `fastcc` and `internal` linkage, so LLVM can inline
  and specialize freely; `pub extern "C"` functions use the C ABI.
- Enum layout, niches and struct field reordering are computed once by
  `codegen/layout` and shared with the borrow checker and the debugger info.
- String literals are private constant globals.
- Vtables are constant globals: `{ drop, size, align, type_id, methods... }`.

Generated IR must be deterministic: identical inputs produce byte-identical
`.ll` files (required for the self-hosting check).

## 12. Linking

The driver writes `out.ll` to a build directory and invokes:

```
clang -O2 out.ll <runtime>/libastra_rt.a -o program      # release
clang -O0 -g out.ll <runtime>/libastra_rt.a -o program   # debug
```

Large programs are split into several `.ll` files (one per group of
modules) compiled by parallel clang processes **[planned]**.

## 13. Diagnostics

```
error[E0382]: use of moved value `inv`
  --> examples/ownership.at:27:5
   |
25 |     discard(inv)
   |             --- value moved here
26 |
27 |     inv.add("potion")
   |     ^^^ value used here after move
   |
   = help: call `inv.clone()` before moving it if you need both copies
```

- Every error has a stable code (`E0001`...) documented in
  `docs/errors/` **[planned]**, with a longer explanation (`astra explain E0382`).
- Errors are sorted by position and deduplicated.
- A machine-readable JSON output (`--message-format=json`) feeds editors
  and the language server.

## 14. Performance targets

| Metric | Target |
|--------|--------|
| Front end (lex → borrow check) | ≥ 200k lines/second per core |
| Self-compile, debug build | < 5 seconds |
| Generated code | within 10% of equivalent C compiled with `clang -O2` |
| Hello world binary (stripped) | < 50 KB |

## 15. Planned evolutions

- Incremental compilation: cache typed and lowered modules keyed by content
  hash.
- Parallel front end per module.
- Astra-level optimizations on AIR before LLVM (inlining of trivial
  functions, removal of redundant checks) to reduce LLVM work.
- A direct machine-code backend for fast debug builds.
