#!/usr/bin/env python3
"""Generates a large, deterministic Astra source file for compiler benchmarks.

The mix of declarations, expressions, strings and comments imitates real
code, so lexer and parser throughput measured on it is representative.

    python3 bench/gen_source.py --lines 100000 > big.at
"""

import argparse
import random

NAMES = ["count", "total", "index", "value", "buffer", "token", "result", "node",
         "width", "height", "name", "items", "offset", "limit", "cursor", "scope"]
TYPES = ["int", "float", "str", "bool", "List<int>", "Map<str, int>", "Option<str>"]


def name(rng: random.Random) -> str:
    return rng.choice(NAMES) + str(rng.randrange(100))


def expression(rng: random.Random, depth: int = 0) -> str:
    choice = rng.randrange(7 if depth < 3 else 3)
    if choice == 0:
        return str(rng.randrange(1_000_000))
    if choice == 1:
        return name(rng)
    if choice == 2:
        return f"{rng.random() * 1000:.3f}"
    if choice == 3:
        op = rng.choice(["+", "-", "*", "/", "%", "==", "<", ">=", "&&", "||"])
        return f"{expression(rng, depth + 1)} {op} {expression(rng, depth + 1)}"
    if choice == 4:
        args = ", ".join(expression(rng, depth + 1) for _ in range(rng.randrange(4)))
        return f"{name(rng)}({args})"
    if choice == 5:
        return f'"{name(rng)} = {{{name(rng)}}} and {{{name(rng)}.len()}}\\n"'
    return f"{name(rng)}.{name(rng)}[{expression(rng, depth + 1)}]"


def statement(rng: random.Random) -> str:
    choice = rng.randrange(6)
    if choice == 0:
        return f"let {name(rng)} = {expression(rng)}"
    if choice == 1:
        return f"var {name(rng)}: {rng.choice(TYPES)} = {expression(rng)}"
    if choice == 2:
        return f"{name(rng)} += {expression(rng)}"
    if choice == 3:
        return f"if {expression(rng)} {{ return {expression(rng)} }}"
    if choice == 4:
        return f"for i in 0..{rng.randrange(1000)} {{ {name(rng)}.push(i) }}"
    return f"// {name(rng)}: adjust the {name(rng)} before the next step"


def function(rng: random.Random) -> list[str]:
    params = ", ".join(f"{name(rng)}: {rng.choice(TYPES)}" for _ in range(rng.randrange(4)))
    lines = [f"/// Computes the {name(rng)} of the {name(rng)}.",
             f"pub fn {name(rng)}({params}) int {{"]
    lines += ["    " + statement(rng) for _ in range(rng.randrange(3, 15))]
    lines += [f"    return {expression(rng)}", "}", ""]
    return lines


def structure(rng: random.Random) -> list[str]:
    lines = [f"pub struct S{rng.randrange(10_000)} {{"]
    lines += [f"    {name(rng)}: {rng.choice(TYPES)}" for _ in range(rng.randrange(2, 8))]
    lines += ["", f"    fn get(self) int {{ return self.{name(rng)} }}", "}", ""]
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--lines", type=int, default=100_000)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    out: list[str] = ["//! Generated benchmark input. Do not edit.", ""]
    while len(out) < args.lines:
        out += structure(rng) if rng.randrange(5) == 0 else function(rng)
    print("\n".join(out[:args.lines]))


if __name__ == "__main__":
    main()
