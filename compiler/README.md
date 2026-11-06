# compiler — stage 1 compiler

The self-hosted Ofir compiler, written in Ofir. Until milestone M4 it must
use only the bootstrap subset defined in [`docs/bootstrap.md`](../docs/bootstrap.md).

Architecture: [`docs/design/compiler-architecture.md`](../docs/design/compiler-architecture.md).

`ofir.json` makes this directory a project: imports resolve from here
(`import base.span`). Each port is checked against stage 0:

```
make stage1           # build/stage1/ofir, built by stage 0
make test-stage1      # every test, with stage 1 as the compiler
make compare-stages   # stage 1's output equals stage 0's on every .of file
```

Stage 1 is a complete compiler with stage 0's command line (`ofir
build|run|test <file.of>`, `--emit=tokens|ast|names|types|air|llvm`,
`-o`, `--release`, `--deny-warnings`, `--message-format=short`,
`--stop-after=lex|parse|check`): `base`, `diag`, the lexer and the parser
(`syntax/`), module loading (`driver/`), the resolver, the type checker and
the move analysis (`sema/`), the AIR and the lowering (`air/`) and the LLVM
codegen (`codegen/`). `fmt/` is `ofir fmt`, which only stage 1 has. It finds std through `OFIR_STD` and the runtime
library through `OFIR_RUNTIME`, or next to its executable, as stage 0.
