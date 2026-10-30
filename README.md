# Astra

Astra is a compiled, statically typed systems programming language with a
lightweight, Go-like syntax and Rust-like memory safety — without a garbage
collector.

> Status: design phase. The language specification lives in [`docs/spec`](docs/spec).

## Goals

- **Simple** — small grammar, no semicolons, type inference, few keywords.
- **Robust** — no null, exhaustive `match`, explicit error handling with
  `Result` and `?`, immutable bindings by default, checked arithmetic.
- **Lightweight** — no GC, no hidden runtime, tiny binaries.
- **Fast** — ownership-based memory management and an LLVM backend.
- **Self-hosted** — the Astra compiler is written in Astra.

## Taste

```astra
struct Point {
    x: float
    y: float
}

fn (p: &Point) length() -> float {
    return sqrt(p.x * p.x + p.y * p.y)
}

fn main() {
    let p = Point{x: 3.0, y: 4.0}
    println("length = {p.length()}")
}
```
