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
| `--message-format=json` | diagnostics as JSON objects, one per line (below) |

### Notes and help

A diagnostic may end with lines that are not about its span: a `note`
gives context, a `help` suggests a fix. The human format writes them
after the excerpt, each on its own line:

```
error[E0304]: cannot assign twice to immutable variable `n`
 --> src/main.of:3:5
  |
3 |     n = 2
  |     ^
  |
  = help: declare it with `var n` to assign to it again
```

The short format leaves them out: it has one line per diagnostic.

### Suggestions

An unknown name (E0200), field (E0316) or method (E0301) gets a help
line naming the closest one that exists, when one is close enough:

```
error[E0200]: cannot find `cuont` in this scope
 --> src/main.of:3:13
  |
3 |     println(cuont)
  |             ^^^^^
  |
  = help: did you mean `count`?
```

- The candidates are the names visible at that point (locals, the
  module's declarations, its imports and the prelude) for a name, the
  type's declared names for a type, the struct's fields for a field and
  the type's methods for a method.
- Closeness is the edit distance between the bytes of the two names
  (insertions, deletions and substitutions). A candidate is close enough
  when the distance is at most a third of the unknown name's length,
  rounded down, so names shorter than three bytes get no suggestion.
- The closest candidate wins; between equally close ones, the name
  interned first. Both compiler stages suggest the same name.

### JSON diagnostics

With `--message-format=json` every diagnostic is written to standard error
as one JSON object per line, sorted as the human format sorts them:

```json
{"severity":"error","code":"E0300","message":"mismatched types: expected int, found str","file":"src/main.of","line":3,"column":13,"endLine":3,"endColumn":18,"notes":[]}
```

- `severity` is `"error"` or `"warning"`; `code` is the registry code
  (`docs/errors`).
- `line` and `column` start at 1 and count bytes; `endLine` and
  `endColumn` are the position just after the span.
- `notes` holds the notes and help lines in order, each an object with
  `kind` (`"note"` or `"help"`) and `message`; it is `[]` without them.
- Driver errors that point at no source, as E0900 (cannot read a file) or
  E0902 (clang failed), have no `file`, `line`, `column`, `endLine` or
  `endColumn`.
- Strings are escaped as JSON requires: `\"`, `\\`, `\n`, `\t` and
  `\u00XX` for other control characters; other bytes are copied.

The format is stage 1's: stage 0 accepts only `short` and `human`.

### `ofir explain`

`ofir explain E0600` prints the explanation of a code: the file
`E0600.md` of `docs/errors`, with what the rule is, an example that
triggers it and how to fix it. The file is looked up in, in order:

1. the directory named by the `OFIR_ERRORS` environment variable;
2. `<prefix>/share/ofir/errors`, next to an installed compiler;
3. the repository's `docs/errors`, for a compiler in `build/stageN`.

The exit status is 0 when the explanation is printed, 1 when no file
has it and 2 when the argument is not a code (`E` or `W` and four
digits). The command is stage 1's.

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
├── lib/ofir/
│   ├── std/                 standard library sources
│   └── libofir_rt.a        runtime
└── share/ofir/errors/       explanations for `ofir explain`
```

## 6. Targets

Version 1 supports 64-bit Linux (x86_64, aarch64), macOS (aarch64,
x86_64) and Windows (x86_64). On Windows the compilers pass
`--target=x86_64-w64-windows-gnu` to clang: the runtime and the programs
use the MinGW C library, so a MinGW-w64 toolchain (for example WinLibs)
must be installed next to clang. Cross-compilation uses
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
On Windows, run `make` from a POSIX shell (Git Bash); `tools/platform.mk`
builds with clang for the MinGW target and finds clang on `PATH` or in
`C:/Program Files/LLVM/bin`.
