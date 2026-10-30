# `std.io`, `std.fs`, `std.os`

## Errors (`std.io`)

```astra
pub struct IoError {
    pub kind: IoErrorKind
    pub message: str
}

pub copy enum IoErrorKind {
    NotFound  PermissionDenied  AlreadyExists  InvalidInput
    UnexpectedEof  Interrupted  Other
}
```

`IoError` satisfies `Display` and `Debug`.

## `std.io`

| Item | Description | S0 |
|------|-------------|:--:|
| `fn stdout() Stdout`, `fn stderr() Stderr` | handles to the runtime-buffered streams | ✓ |
| `fn stdin() Stdin` | | ✓ |
| `fn (s: &mut Stdin) read_line(buf: &mut str) Result<int, IoError>` | returns bytes read, `0` at end of input | ✓ |
| `fn (s: &mut Stdin) read_to_string() Result<str, IoError>` | | ✓ |
| `interface Reader`, `interface Writer` | spec 9.4 | |
| `struct BufWriter<W>` | buffered writer over any `Writer` | |
| `fn flush() Result<(), IoError>` | flush standard output | ✓ |

## `std.fs`

| Function | Description | S0 |
|----------|-------------|:--:|
| `fn read_to_string(path: &str) Result<str, IoError>` | | ✓ |
| `fn read(path: &str) Result<List<u8>, IoError>` | | ✓ |
| `fn write(path: &str, data: &[u8]) Result<(), IoError>` | creates or truncates | ✓ |
| `fn exists(path: &str) bool` | | ✓ |
| `fn create_dir_all(path: &str) Result<(), IoError>` | | ✓ |
| `fn remove_file(path: &str) Result<(), IoError>` | | ✓ |
| `fn read_dir(path: &str) Result<List<DirEntry>, IoError>` | sorted by name, for determinism | ✓ |
| `struct File` with `open`, `create`, `read`, `write`, closed on drop | | |

`std.fs.path`: `join(a: &str, b: &str) str`, `parent(p: &str) Option<&str>`,
`file_name(p: &str) Option<&str>`, `extension(p: &str) Option<&str>` — all S0.

## `std.os`

| Function | Description | S0 |
|----------|-------------|:--:|
| `fn args() List<str>` | `args()[0]` is the program path | ✓ |
| `fn env(name: &str) Option<str>` | | ✓ |
| `fn exit(code: int) !` | flushes standard output, runs no destructors | ✓ |
| `fn run(program: &str, args: &[&str]) Result<Output, IoError>` | runs a process and waits | ✓ |
| `fn spawn(program: &str, args: &[&str]) Result<Child, IoError>` | | |
| `fn current_dir() Result<str, IoError>` | | ✓ |
| `fn executable_path() Result<str, IoError>` | locates `ASTRA_HOME` | ✓ |

```astra
pub struct Output {
    pub status: int        // exit status, or -1 if killed by a signal
    pub stdout: str
    pub stderr: str
}
```
