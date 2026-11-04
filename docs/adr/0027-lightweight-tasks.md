# 0027. Concurrency through lightweight tasks, without `async`/`await`

- Status: Accepted
- Date: 2026-11-04

## Context

Spec chapter 14 fixes the base model: tasks started with `task.run`, each on
an OS thread of its own, which communicate through channels. `Send` and
`Sync` prevent data races at compile time. The model for many concurrent
tasks, such as a server with thousands of connections, was left open:

- **`async`/`await`** (Rust, C#, JavaScript): an `async fn` compiles to a
  state machine, and an executor polls it. It is zero cost and needs no
  runtime. But functions get colors: an `async fn` is awaited only from
  another `async fn`, so `std` and libraries split in two. The compiler
  must lower every `async fn` to a state machine in the AIR, check borrows
  held across an `await` (Rust's `Pin`) and ship an executor.
- **Lightweight tasks** (Go, Erlang, Java's virtual threads): each task has
  a small stack of its own. A scheduler in the runtime runs many tasks on
  few OS threads (M:N) and switches tasks when one blocks on I/O. Blocking
  code reads as ordinary code, and functions have no color. The cost is a
  runtime: task stacks, the scheduler and non-blocking I/O in `std`.

## Decision

Ofir has **no `async`/`await`**. Concurrency goes through **tasks**:

- `task.run(move () => ...)` starts a task and returns its handle.
  `handle.join()` waits for the task and returns its result.
- Functions have no color: the same function runs in a task or outside one.
- Tasks first run on OS threads (spec 14.2). A later M:N scheduler makes
  them lightweight without changing the API: the same `task.run`, channels
  and `std.sync`.
- The scheduler and task stacks belong to `std.task` and its runtime. A
  program that starts no task links none of it.

## Consequences

- There is a single `std`, with no `async` duplicates. `std.io`, `std.fs`
  and `std.net` can become non-blocking under the scheduler without
  changing their signatures.
- The compiler needs no state machine transformation and no borrow rules
  across suspension points: `Send`/`Sync` and `move` closures are enough.
- The M:N scheduler needs runtime work: growable or large virtually
  reserved stacks, context switches, and an event loop (epoll, kqueue,
  IOCP).
- A C call that blocks (FFI) holds its OS thread. The scheduler must
  detect it or move it to a thread of its own, as Go does.
- A task costs more memory than a Rust future: a stack of its own, in
  place of a state machine sized at compile time.
