# bootstrap — stage 0 compiler

The stage 0 compiler, written in C11. It implements the bootstrap subset of
Astra (see [`docs/bootstrap.md`](../docs/bootstrap.md)) and exists only to
compile the self-hosted compiler in [`compiler/`](../compiler).

```
make            # debug build with sanitizers → ../build/stage0/astra
make release    # optimized build
make test       # C unit tests
```

Layout and dependency rules: [`docs/design/project-structure.md`](../docs/design/project-structure.md).
