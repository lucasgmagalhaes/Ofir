# 0030. `Send` and `Sync` are derived, closures carry `+ Send`

- Status: Accepted
- Date: 2026-11-06

## Context

`task.run` (spec 14.2) moves a closure to another OS thread. Without a
check, the closure can capture a value that two threads must not share,
such as a struct holding a raw pointer. Spec 14.3 planned two marker
interfaces, `Send` and `Sync`. Three questions had to be settled:

1. **Where the check lives.** A closure's captures are not part of its
   type: `move fn() int` says nothing about what it holds. Either the
   check reads the closure where it is converted, or the function type
   records it.
2. **How the std types that hold raw pointers (`List`, `Box`, `str`) are
   `Send`.** Derivation alone says they are not.
3. **What a borrowing closure needs.** It shares its captures with the
   task that calls it.

## Decision

- `Send` and `Sync` are derived from the structure of a type, never
  implemented by hand. Raw pointers and `dyn I` are neither; a type
  parameter is what its bounds say.
- A function type can add `+ Send`. A closure converts to it only if a
  `move` closure's captures are all `Send`, or a borrowing closure's are
  all `Sync` (E0360). The checked type is carried by the function value,
  so a `+ Send` function passed on needs no second check, and
  `fn() T + Send` converts to `fn() T`. No function type is `Sync`.
- `@threadsafe` on a struct or enum makes it `Send` (`Sync`) when its type
  arguments are, whatever its fields hold. It is a promise, like
  `unsafe`: the std types built on raw pointers use it.
- `task.run<T: Send>(f: move fn() T + Send)`: the result moves back too.

Rust's `unsafe impl Send for T` was not taken: Ofir has no negative or
manual impls of marker interfaces, and one attribute covers the std types.
A `Send` bound on the closure's type parameter (`F: Send`) was not taken
either: Ofir closures have no type of their own to bound.

## Consequences

- A capture that is unsafe to move to another task is a compile error.
- A user type built on raw pointers needs `@threadsafe` to reach a task;
  the compiler trusts the marker.
- Stage 0 has no tasks, so it accepts `@threadsafe` and checks nothing.
- `Rc<T>`, when it exists, must not be `@threadsafe`; `Arc<T>` (6.1d)
  will be.
