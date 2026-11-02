# Toolchain and build process

## 1. The `ofir` command

A single executable provides every tool:

| Command | Description |
|---------|-------------|
| `ofir new <name>` | create a project with `ofir.json` and `src/main.of` |
| `ofir build [--release]` | compile the project to an executable |
| `ofir run [--release] [-- args]` | build and run |
| `ofir check` | run every phase up to the borrow checker, no code generation |
| `ofir test [filter]` | build and run `@test` functions and doc tests |
| `ofir bench [filter]` | build with optimizations and run `@bench` functions |
| `ofir fmt [--check]` | format source files **[planned]** |
| `ofir doc` | generate HTML documentation **[planned]** |
| `ofir explain <code>` | describe an error code |
| `ofir version` | print version and LLVM/clang versions in use |

A single file can be compiled without a project: `ofir run hello.of`.

### Debug flags (for compiler developers)

| Flag | Effect |
|------|--------|
| `--emit=tokens` | print tokens and stop |
| `--emit=ast` | print the AST and stop |
| `--emit=types` | print the types of the root module's declarations and locals, and stop |
| `--emit=names` | print what the root module's names resolve to (locals, declarations, imports) and stop |
| `--emit=air` | print AIR after drop elaboration |
| `--emit=llvm` | write the `.ll` file next to the output |
| `--emit=asm` | write assembly |
| `--time-passes` | print the time spent in each phase |
| `--message-format=short` | one line per diagnostic: `file:line:col: error[E0300]: message` |
| `--message-format=json` | diagnostics as JSON objects **[planned]** |

## 2. Project layout

```
my_app/
├── ofir.json
├── src/
│   ├── main.of             root module of an executable
│   └── net/
│       └── http.of         module net.http
└── build/                  generated, ignored by git
    ├── debug/
    └── release/
```

### `ofir.json`

The manifest is JSON with comments (`//`, `/* */`) and trailing commas
allowed, the format of `tsconfig.json` (ADR 0026). Keys are camelCase.

```jsonc
{
  "name": "my_app",
  "version": "0.1.0",
  "edition": "2027",
  "build": {
    "kind": "exe",            // "exe" or "lib"
    "root": "src/main.of",
  },
  "dependencies": {},         // [planned] package manager
  "fmt": {                    // what `ofir fmt` produces (docs/style-guide.md)
    "inlineBlocks": false,    // a function or statement body never shares a line with its braces
  },
}
```

`fmt` properties:

| Property | Default | Meaning |
|----------|---------|---------|
| `inlineBlocks` | `false` | when `false`, the body of every function and of every statement (`if`, `else`, `for`, `unsafe`, `match` arm, `let ... else`) is broken onto its own lines; an `if` or `unsafe` used as a value may stay on one line if each block holds one expression. `true` lets a body of a single statement stay on one line. |

Until `ofir fmt` exists, `tools/check_format.py` checks the repository
against the default, and `--fix` applies it.

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
1. read ofir.json, locate the root module
2. parse the root module and, transitively, every imported module
3. check (resolve, type check, borrow check) all modules
4. lower, monomorphize and generate build/<profile>/<name>.ll
5. clang <flags> <name>.ll libofir_rt.a -o build/<profile>/<name>
```

The compiler finds:

- the **standard library** in `$OFIR_HOME/std` (default: next to the
  executable, `../lib/ofir/std`);
- the **runtime** in `$OFIR_HOME/lib/libofir_rt.a`;
- **clang** through `$OFIR_CLANG`, otherwise `clang` on the `PATH`.
- In stage 0, which has no projects yet: the runtime through
  `$OFIR_RUNTIME`, otherwise `libofir_rt.a` in the parent directory of the
  compiler (`build/libofir_rt.a` for `build/stage0/ofir`). `ofir build
  file.of` writes `./file` (or `-o`); `ofir run file.of` builds into a
  temporary directory and runs the program.
  Supported: LLVM 17 to the latest release, each tested in CI (ADR 0001).
  `ofir version` reports the clang in use.

## 5. Installation layout

```
<prefix>/
├── bin/ofir
└── lib/ofir/
    ├── std/                 standard library sources
    └── libofir_rt.a        runtime
```

## 6. Targets

Version 1 supports 64-bit Linux (x86_64, aarch64) and macOS (aarch64,
x86_64). Windows support is **[planned]**. Cross-compilation uses
`--target=<llvm triple>` and relies on clang's cross-compilation support.

## 7. Repository build (for contributors)

```
make stage0        # build bootstrap/ with the system C compiler → build/stage0/ofir
make runtime       # build runtime/ → build/libofir_rt.a
make stage1        # compile compiler/ with stage0 → build/stage1/ofir
make stage2        # compile compiler/ with stage1 → build/stage2/ofir
make verify        # stage2 compiles compiler/ again; outputs must be identical
make test          # run all suites in tests/ against the newest stage
```

Requirements: a C11 compiler, `make`, clang ≥ 17, and Python 3 for the
test runner (a development dependency only; users never need Python).
