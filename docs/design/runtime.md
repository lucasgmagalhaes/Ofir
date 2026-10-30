# Runtime

The runtime is a small C library (`runtime/`) linked into every Astra
program. It contains only what cannot be written in Astra or in portable
LLVM IR. Its interface is `runtime/include/astra_rt.h`.

## 1. Responsibilities

| Area | Functions |
|------|-----------|
| Entry | `int main(int argc, char **argv)` stores the arguments and calls the Astra entry point `astra_main` |
| Panics | `astra_panic(msg, len, file, line, col)` and specialized cold functions for common checks |
| Memory | `astra_alloc`, `astra_realloc`, `astra_free` (wrapping `malloc`/`realloc`/`free`, panicking on failure) |
| I/O | `astra_write(fd, ptr, len)`, `astra_read(fd, ptr, len)` |
| Process | `astra_exit(code)`, `astra_args_count()`, `astra_arg(i)` |
| Files | thin wrappers over `open`, `close`, `stat`, `opendir` |
| Processes | `astra_spawn` (needed for the compiler to invoke clang) |

## 2. Panic functions

Generated code calls specialized functions so that the call sites stay
small and the messages are consistent:

```c
_Noreturn void astra_panic(const uint8_t *msg, size_t len, const AstraLoc *loc);
_Noreturn void astra_panic_bounds(size_t index, size_t len, const AstraLoc *loc);
_Noreturn void astra_panic_overflow(const AstraLoc *loc);
_Noreturn void astra_panic_div_zero(const AstraLoc *loc);
_Noreturn void astra_panic_unwrap_none(const AstraLoc *loc);

typedef struct { const char *file; uint32_t line, col; } AstraLoc;
```

`AstraLoc` values are constant globals emitted by codegen. All panic
functions are `cold` and `noreturn`, so they do not slow down the fast path.

A panic:

1. flushes buffered standard output;
2. prints `panic: <message>` and `  at <file>:<line>:<col>` to standard error;
3. exits with status 101.

## 3. Rules

- The runtime has no global constructors and no threads.
- It never calls back into Astra code, except `astra_main`.
- Every function is prefixed `astra_` to avoid clashes with user code.
- It must compile without warnings with `-std=c11 -Wall -Wextra -Werror`.

## 4. Evolution

As the standard library grows, functions move from C to Astra using
`extern "C"` declarations of libc functions. The long-term goal is a runtime
containing only the process entry point and the panic handler.
