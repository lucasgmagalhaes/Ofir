# 14. Concurrency **[planned]**

Concurrency is not part of the bootstrap language, but the design is fixed
now so that earlier features do not block it.

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
takes a `move fn() T` (chapter 10.2), so a task can never hold a reference
into another task's stack.

## 14.3 Marker interfaces

Two built-in marker interfaces (no methods), derived automatically:

| Interface | Meaning |
|-----------|---------|
| `Send` | values of the type may be moved to another task |
| `Sync` | shared references to the type may be used from several tasks |

Raw pointers and `Rc<T>` are neither `Send` nor `Sync`. A struct is `Send`
when all its fields are `Send`. `task.run` requires its closure to be
`Send`.

## 14.4 Communication

```ofir
let (tx, rx) = channel<int>()
task.run(move () => tx.send(42))
println("{rx.recv().unwrap()}")
```

Synchronization primitives in `std.sync`: `Mutex<T>` (the data lives inside
the mutex and is reachable only while the lock is held), `RwLock<T>`,
`Atomic*` types, `Arc<T>` (atomically reference counted) and `WaitGroup`.
