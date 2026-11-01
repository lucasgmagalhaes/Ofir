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
| I/O | `astra_write(fd, ptr, len)`, `astra_read(fd, ptr, len)`, the standard output buffer (`astra_stdout_write`, `astra_stdout_flush`) |
| Crashes | stack overflow detection (section 3) |
| Process | `astra_exit(code)`, `astra_arg_count()`, `astra_arg(i, &len)`, `astra_env` |
| Files | thin wrappers over `open`, `read`, `write`, `stat`, `mkdir`, `unlink`, `opendir`, `getline` |
| Processes | `astra_run`: `posix_spawnp` with both output streams captured (needed for the compiler to invoke clang) |

Stage 0 implements the entry point, the panic functions, the allocator,
the standard output buffer and the string functions used by
interpolation (`runtime/src`). Values of type `str` are passed to the
runtime by pointer, which keeps the C ABI of generated calls simple.

The system functions (`os.c`, `fs.c`) serve std.os, std.fs and std.io,
which call them through `extern "C"` blocks: a string goes in as a
pointer and a length (the runtime adds the NUL that C wants, and rejects
a string that holds one with `EINVAL`), and a result comes back in an
`AstraBuf` the runtime allocates with `astra_alloc`, which std takes over
as a `str` or a `List<u8>` without copying. They return 0 or an `errno`
value; `astra_error_kind` and `astra_error_message` turn it into an
`IoError`.

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

1. flushes the standard output buffer (owned by the runtime, see below);
2. prints `panic: <message>` and `  at <file>:<line>:<col>` to standard error;
3. exits with status 101.

### Standard output buffer

The buffer for standard output lives **in the runtime**, not in `std.io`,
so that the runtime can flush it on panic and at exit without calling back
into Astra code. It is line buffered when standard output is a terminal and
block buffered (64 KiB) otherwise. Writing to standard error first flushes
standard output, so messages keep their order.

## 3. Stack overflow

Deep recursion must not end in an anonymous segmentation fault. At startup
the runtime installs a `SIGSEGV` handler on an alternate signal stack
(`sigaltstack`). When a fault address lies in the guard page below the main
thread's stack, the handler prints

```
panic: stack overflow
```

and exits with status 101, like any other panic. Other faults are re-raised
unchanged.

Stage 0 (`runtime/src/stack.c`) takes the address of a local of `main` as
the top of the stack and the soft `RLIMIT_STACK` (8 MiB when unlimited)
as its size; a fault from there down to 1 MiB past the limit, which
covers the guard page and the frames that jump over it, is an overflow.
It handles `SIGBUS` too, which macOS raises for some overflows.

## 4. Rules

- The runtime has no global constructors and no threads.
- It never calls back into Astra code, except `astra_main`.
- Every function is prefixed `astra_` to avoid clashes with user code.
- It must compile without warnings with `-std=c11 -Wall -Wextra -Werror`.

## 5. Evolution

As the standard library grows, functions move from C to Astra using
`extern "C"` declarations of libc functions. The long-term goal is a runtime
containing only the process entry point and the panic handler.
