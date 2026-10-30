# Toolchain and build process

## 1. The `astra` command

A single executable provides every tool:

| Command | Description |
|---------|-------------|
| `astra new <name>` | create a project with `astra.toml` and `src/main.at` |
| `astra build [--release]` | compile the project to an executable |
| `astra run [--release] [-- args]` | build and run |
| `astra check` | run every phase up to the borrow checker, no code generation |
| `astra test [filter]` | build and run `@test` functions and doc tests |
| `astra bench [filter]` | build with optimizations and run `@bench` functions |
| `astra fmt [--check]` | format source files **[planned]** |
| `astra doc` | generate HTML documentation **[planned]** |
| `astra explain <code>` | describe an error code |
| `astra version` | print version and LLVM/clang versions in use |

A single file can be compiled without a project: `astra run hello.at`.

### Debug flags (for compiler developers)

| Flag | Effect |
|------|--------|
| `--emit=tokens` | print tokens and stop |
| `--emit=ast` | print the AST and stop |
| `--emit=air` | print AIR after drop elaboration |
| `--emit=llvm` | write the `.ll` file next to the output |
| `--emit=asm` | write assembly |
| `--time-passes` | print the time spent in each phase |

## 2. Project layout

```
my_app/
├── astra.toml
├── src/
│   ├── main.at             root module of an executable
│   └── net/
│       └── http.at         module net.http
└── build/                  generated, ignored by git
    ├── debug/
    └── release/
```

### `astra.toml`

```toml
[package]
name = "my_app"
version = "0.1.0"
edition = "2027"

[build]
kind = "exe"            # "exe" or "lib"
root = "src/main.at"

[dependencies]          # [planned] package manager
```

## 3. Build profiles

| Profile | clang flags | Checks | Debug info |
|---------|-------------|--------|------------|
| `debug` (default) | `-O0 -g` | all | yes |
| `release` | `-O2` | all (checked arithmetic and bounds stay on) | no |
| `release-fast` **[planned]** | `-O3 -march=native` + LTO | all | no |

Safety checks are never disabled by a profile. The optimizer removes the
checks it can prove unnecessary.

## 4. Build steps

```
1. read astra.toml, locate the root module
2. parse the root module and, transitively, every imported module
3. check (resolve, type check, borrow check) all modules
4. lower, monomorphize and generate build/<profile>/<name>.ll
5. clang <flags> <name>.ll libastra_rt.a -o build/<profile>/<name>
```

The compiler finds:

- the **standard library** in `$ASTRA_HOME/std` (default: next to the
  executable, `../lib/astra/std`);
- the **runtime** in `$ASTRA_HOME/lib/libastra_rt.a`;
- **clang** through `$ASTRA_CLANG`, otherwise `clang` on the `PATH`.
  Minimum version: LLVM 15 (opaque pointers).

## 5. Installation layout

```
<prefix>/
├── bin/astra
└── lib/astra/
    ├── std/                 standard library sources
    └── libastra_rt.a        runtime
```

## 6. Targets

Version 1 supports 64-bit Linux (x86_64, aarch64) and macOS (aarch64,
x86_64). Windows support is **[planned]**. Cross-compilation uses
`--target=<llvm triple>` and relies on clang's cross-compilation support.

## 7. Repository build (for contributors)

```
make stage0        # build bootstrap/ with the system C compiler → build/stage0/astra
make runtime       # build runtime/ → build/libastra_rt.a
make stage1        # compile compiler/ with stage0 → build/stage1/astra
make stage2        # compile compiler/ with stage1 → build/stage2/astra
make verify        # stage2 compiles compiler/ again; outputs must be identical
make test          # run all suites in tests/ against the newest stage
```

Requirements: a C11 compiler, `make`, clang ≥ 15, and Python 3 for the
test runner.
