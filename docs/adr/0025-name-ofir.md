# 0025. The language is named Ofir

- Status: Accepted
- Date: 2026-11-02

## Context

The language was developed as Astra, with `.at` source files. The name
is taken by other projects, and the project chose a name of its own.

## Decision

The language is **Ofir**, and its source files end in **`.of`**:

- the compiler is `ofir` (`ofir build`, `ofir run`, `ofir test`);
- a project root has an `ofir.toml`; a module is `name.of` or
  `name/index.of`, its tests `name_test.of`;
- the environment variables are `OFIR_STD`, `OFIR_CLANG`, `OFIR_HOME`;
- the runtime library is `libofir_rt.a` (`runtime/include/ofir_rt.h`), and
  its symbols start with `ofir_` (`ofir_print_str`, `ofir_panic`).

The repository, its history and the earlier ADRs keep their content; their
text uses the new name.

## Consequences

- Every source, tool, test and document was renamed at once; programs
  written for Astra need their files renamed to `.of`.
- `tools/bench_compare.sh` still finds the compiler of a base commit from
  before the rename (`astra`), so that its benchmarks can be compared.
