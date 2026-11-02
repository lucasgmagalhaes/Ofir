# compiler — stage 1 compiler

The self-hosted Ofir compiler, written in Ofir. Until milestone M4 it must
use only the bootstrap subset defined in [`docs/bootstrap.md`](../docs/bootstrap.md).

Architecture: [`docs/design/compiler-architecture.md`](../docs/design/compiler-architecture.md).

`ofir.json` makes this directory a project: imports resolve from here
(`import base.span`). Each port is checked against stage 0:

```
make test-std         # the @test functions of compiler/ (and std)
make compare-stages   # stage 1's output equals stage 0's on every .of file
```

So far stage 1 has `base`, `diag`, the lexer, the parser, module loading
(`driver/modules.of`), the resolver, the type checker and the move analysis
(`sema/`), the AIR and the lowering (`air/`) and the LLVM codegen (`codegen/`);
`main.of` has the commands `main tokens|ast|names|types|air|llvm <file.of>`,
the equivalents of `ofir build --emit=tokens`, `--emit=ast`, `--emit=names`,
`--emit=types`, `--emit=air` and `--emit=llvm`, and `main build|run <file.of>`,
which compile with clang (`-o`, `--deny-warnings`, `--message-format=short`;
the runtime library is found through `OFIR_RUNTIME`). `main air` exits with
status 3 on a program that uses a construct it does not lower yet.
