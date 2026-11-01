# compiler — stage 1 compiler

The self-hosted Astra compiler, written in Astra. Until milestone M4 it must
use only the bootstrap subset defined in [`docs/bootstrap.md`](../docs/bootstrap.md).

Architecture: [`docs/design/compiler-architecture.md`](../docs/design/compiler-architecture.md).

`astra.toml` makes this directory a project: imports resolve from here
(`import base.span`). Each port is checked against stage 0:

```
make test-std         # the @test functions of compiler/ (and std)
make compare-stages   # stage 1's output equals stage 0's on every .at file
```

So far stage 1 has `base`, `diag`, the lexer, the parser, module loading
(`driver/modules.at`) and the resolver (`sema/`); `main.at` has three
commands, `main tokens|ast|names <file.at>`, the equivalents of `astra
build --emit=tokens`, `--emit=ast` and `--emit=names`.
