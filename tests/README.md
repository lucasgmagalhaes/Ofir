# tests — test suites

| Suite | Content |
|-------|---------|
| `lex/` | `*.at` + `*.tokens`: expected token streams |
| `parse/` | `*.at` + `*.ast`: expected AST dumps |
| `run/` | `*.at` + `*.out`: programs and their expected standard output |
| `compile_fail/` | `*.at` with `// error[EXXXX]: message` annotations |
| `panic/` | `*.at` with a `// panic: message` header |
| `borrow/` | ownership and borrow checker cases |
| `codegen/` | `*.at` with `// CHECK:` lines matched against the LLVM IR |
| `selfhost/` | stage 2 == stage 3 verification |

Run everything with `make test`, or a subset with
`python3 tests/runner/run_tests.py --compiler build/stage0/astra run/structs`.
Formats and rules: [`docs/design/testing.md`](../docs/design/testing.md).
