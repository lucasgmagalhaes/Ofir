# tests — test suites

| Suite | Content |
|-------|---------|
| `lex/` | `*.of` + `*.tokens`: expected token streams |
| `parse/` | `*.of` + `*.ast`: expected AST dumps |
| `run/` | `*.of` + `*.out`: programs and their expected standard output |
| `compile_fail/` | `*.of` with `// error[EXXXX]: message` annotations |
| `panic/` | `*.of` with a `// panic: message` header |
| `borrow/` | ownership and borrow checker cases |
| `codegen/` | `*.of` with `// CHECK:` lines matched against the LLVM IR |
| `selfhost/` | stage 2 == stage 3 verification |

Run everything with `make test`, or a subset with
`python3 tests/runner/run_tests.py --compiler build/stage0/ofir run/structs`.
Formats and rules: [`docs/design/testing.md`](../docs/design/testing.md).
