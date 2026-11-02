# 14. Concurrency **[planned]**

Concurrency is not part of the bootstrap language, but the design is fixed
now so that earlier features do not block it.

## 14.1 Goals

- **No data races in safe code**, enforced at compile time, as with Rust.
- **No mandatory runtime**: programs that do not use concurrency pay
  nothing for it.
- **Simple model first**: OS threads and channels. Lightweight tasks
  (green threads or async) may come later as a library.

## 14.2 Threads

```ofir
import std.thread

fn main() {
    let data = [1, 2, 3]
    let handle = thread.spawn(move () int => {
        var sum = 0
        for x in data {
            sum += x
        }
        return sum
    })
    println("{handle.join()}")
}
```

`thread.spawn` takes a `move fn() T` (chapter 10.2), so a thread can never
hold a reference into another thread's stack.

## 14.3 Marker interfaces

Two built-in marker interfaces (no methods), derived automatically:

| Interface | Meaning |
|-----------|---------|
| `Send` | values of the type may be moved to another thread |
| `Sync` | shared references to the type may be used from several threads |

Raw pointers and `Rc<T>` are neither `Send` nor `Sync`. A struct is `Send`
when all its fields are `Send`. `thread.spawn` requires its closure to be
`Send`.

## 14.4 Communication

```ofir
let (tx, rx) = channel<int>()
thread.spawn(move () => tx.send(42))
println("{rx.recv().unwrap()}")
```

Synchronization primitives in `std.sync`: `Mutex<T>` (the data lives inside
the mutex and is reachable only while the lock is held), `RwLock<T>`,
`Atomic*` types, `Arc<T>` (atomically reference counted) and `WaitGroup`.
