# 14. Concurrency

Concurrency is not part of the bootstrap language. Tasks (14.2) are
implemented by stage 1 only; the rest of the chapter is **[planned]**, and
its design is fixed now so that earlier features do not block it.

## 14.1 Goals

- **No data races in safe code**, enforced at compile time, as with Rust.
- **No mandatory runtime**: programs that do not use concurrency pay
  nothing for it.
- **Simple model first**: tasks that communicate through channels, each
  task on an OS thread of its own. A later M:N scheduler makes tasks
  lightweight behind the same `task.run` API.
- **No `async`/`await`** (ADR 0027): functions have no color; the same
  function runs in a task or outside one.

## 14.2 Tasks

```ofir
import std.task

fn main() {
    let data = [1, 2, 3]
    let handle = task.run(move () int => {
        var sum = 0
        for x in data {
            sum += x
        }
        return sum
    })
    println("{handle.join()}")
}
```

`task.run` starts a task and returns its handle; `join` waits for the task
and returns its result. A task runs on an OS thread of its own. `task.run`
takes a `move fn() T + Send` (chapter 10.2, 14.3), so a task can never hold
a reference into another task's stack, nor a value that is unsafe to use
from another thread.

`run` returns a `task.Task<T>`. A task that panics ends the whole program,
as a panic on the main thread does. The output of `print` and `println`
reaches standard output a line at a time, so lines from different tasks
never interleave.

## 14.3 Marker interfaces

Two built-in marker interfaces (no methods), derived from the structure of
a type:

| Interface | Meaning |
|-----------|---------|
| `Send` | values of the type may be moved to another task |
| `Sync` | shared references to the type may be used from several tasks |

- Numbers, `bool`, `str`, `()`, `never` and key types are both.
- Raw pointers (`*T`, `*mut T`) and `dyn I` are neither.
- `&T` is `Send` when `T` is `Sync`; `&mut T` is `Send` when `T` is `Send`.
  Both are `Sync` when `T` is `Sync`.
- A slice, array or tuple is `Send` (`Sync`) when its elements are.
- A struct or enum is `Send` (`Sync`) when all its fields are; a recursive
  type counts as `Send` (`Sync`) where it reaches itself again.
- A struct or enum marked `@threadsafe` (chapter 12) is `Send` (`Sync`)
  when its type arguments are, whatever its fields hold. The marker is how
  a type built on raw pointers, such as `List<T>`, `Box<T>` or `str`,
  states that it owns what they reach. The compiler cannot check it, so it
  belongs to code that is correct under `unsafe` rules (chapter 11).
- A type parameter is `Send` (`Sync`) when it has that bound:
  `fn spawn<T: Send>(x: move T)`.

A function type may add `+ Send`: `move fn() T + Send`. A closure
converts to it only if it may run on another task: a `move` closure moves
its captures, which must all be `Send`; a borrowing closure shares them,
which must all be `Sync` (E0360). A named function captures nothing and
converts to any `+ Send` type. `fn() T + Send` converts to `fn() T`, and
no function type is `Sync`.

```ofir
import std.task

fn twice(f: fn() int + Send) int {
    return f() + f()
}

fn main() {
    let n = 21
    println("{twice(() int => n)}")
    let t = task.run(move () int => n * 2)
    println("{t.join()}")
}
```

`task.run` takes a `move fn() T + Send` and requires `T: Send`, since the
result moves back to the task that calls `join`. Stage 0 has no tasks: it
accepts `@threadsafe` and does not check `Send`.

## 14.4 Communication

```ofir
import std.sync.channel
import std.task

fn main() {
    let (tx, rx) = channel<int>()
    let t = task.run(move () => tx.send(42))
    println("{rx.recv().unwrap()}")
    t.join()
}
```

`channel<T: Send>()` in `std.sync` returns a `Sender<T>` and a
`Receiver<T>`. The channel is unbounded: `send` never waits, and it drops
the value if the receiver is gone. `clone` on a sender makes another
sender of the same channel. `recv` waits for the next value and returns
`None` once the queue is empty and every sender is dropped; `tryRecv`
returns `None` instead of waiting. The values of one sender arrive in the
order it sent them. Dropping the receiver drops the values it never took.

**[planned]** Synchronization primitives in `std.sync`: `Mutex<T>` (the data lives inside
the mutex and is reachable only while the lock is held), `RwLock<T>`,
`Atomic*` types, `Arc<T>` (atomically reference counted) and `WaitGroup`.
