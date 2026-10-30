# Architecture Decision Records

Each record documents one significant decision: its context, the decision,
and its consequences. Records are immutable once accepted; a decision is
changed by writing a new record that supersedes the old one.

| # | Decision | Status |
|---|----------|--------|
| [0001](0001-textual-llvm-ir-backend.md) | Generate textual LLVM IR and compile it with clang | Accepted |
| [0002](0002-ownership-with-second-class-references.md) | Ownership with second-class references, no lifetime annotations | Accepted |
| [0003](0003-self-hosting-with-c-bootstrap.md) | Self-hosted compiler bootstrapped from a C compiler | Accepted |
| [0004](0004-implicit-interfaces.md) | Implicit (structural) interfaces with static and dynamic dispatch | Accepted |
| [0005](0005-checked-arithmetic.md) | Integer arithmetic is checked in every build profile | Accepted |
| [0006](0006-errors-as-values.md) | Errors as values (`Result`, `?`), panics abort | Accepted |
| [0007](0007-monomorphized-generics.md) | Generics implemented by monomorphization | Accepted |
| [0008](0008-go-like-syntax.md) | Go-like syntax without semicolons | Accepted (termination rule superseded by 0009) |
| [0009](0009-parser-driven-newlines.md) | Parser-driven statement termination | Accepted |
| [0010](0010-non-escaping-function-parameters.md) | Non-escaping function values by default | Accepted |
| [0011](0011-ordered-deterministic-map.md) | Insertion-ordered `Map` with deterministic hashing | Accepted |
| [0012](0012-int-for-lengths-and-indices.md) | `int` for lengths and indices | Accepted |

## Template

```markdown
# NNNN. Title

- Status: Proposed | Accepted | Superseded by NNNN
- Date: YYYY-MM-DD

## Context
## Decision
## Consequences
```
