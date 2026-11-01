#!/usr/bin/env python3
"""Generates a large, deterministic Astra source file for compiler benchmarks.

The mix of declarations, expressions, strings and comments imitates real
code, so lexer and parser throughput measured on it is representative.

The `typed` profile generates only programs that the type checker accepts
(the subset stage 0 supports so far), for the benchmarks of later phases.

    python3 bench/gen_source.py --lines 100000 > big.at
    python3 bench/gen_source.py --profile typed --lines 100000 > typed.at
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
        binary = f"{expression(rng, depth + 1)} {op} {expression(rng, depth + 1)}"
        return f"({binary})" if depth > 0 else binary   # comparisons do not chain
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


def int_expr(rng: random.Random, ints: list[str], depth: int = 0) -> str:
    choice = rng.randrange(4 if depth < 2 else 2)
    if choice == 0:
        return str(rng.randrange(1000))
    if choice == 1:
        return rng.choice(ints)
    op = rng.choice(["+", "-", "*", "/", "%", "&", "|", "^"])
    expr = f"{int_expr(rng, ints, depth + 1)} {op} {int_expr(rng, ints, depth + 1)}"
    return f"({expr})" if depth > 0 else expr


def typed_statement(rng: random.Random, ints: list[str], var: str) -> str:
    choice = rng.randrange(6)
    if choice == 0:
        # `_` keeps the input free of unused-variable warnings (W0001).
        name = f"_{rng.choice(NAMES)}{len(ints)}"
        line = f"let {name} = {int_expr(rng, ints)}"
        ints.append(name)
        return line
    if choice == 1:
        return f"{var} += {int_expr(rng, ints)}"
    if choice == 2:
        return f"if {int_expr(rng, ints)} > {rng.randrange(100)} {{ {var} -= 1 }}"
    if choice == 3:
        return f"for i in 0..{rng.randrange(100)} {{ {var} += i * 2 }}"
    if choice == 4:
        return f'println("{var} = {{{var}}}, flag = {{flag}}")'
    return f"// adjust the {rng.choice(NAMES)} before the next step"


def typed_function(rng: random.Random, index: int, previous: int) -> list[str]:
    ints = ["a", "b"]
    lines = [f"/// Step {index} of the generated pipeline.",
             f"fn step{index}(a: int, b: int, flag: bool) int {{",
             f"    var acc = {int_expr(rng, ints)}"]
    ints.append("acc")
    lines += ["    " + typed_statement(rng, ints, "acc") for _ in range(rng.randrange(3, 15))]
    call = f" + step{previous}(acc, {int_expr(rng, ints)}, !flag)" if previous >= 0 else ""
    lines += [f"    return acc{call}", "}", ""]
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--lines", type=int, default=100_000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--profile", choices=["syntax", "typed"], default="syntax")
    args = parser.parse_args()
    rng = random.Random(args.seed)
    out: list[str] = ["//! Generated benchmark input. Do not edit.", ""]
    if args.profile == "typed":
        count = 0
        while len(out) < args.lines:   # whole functions only: the output must type check
            out += typed_function(rng, count, count - 1)
            count += 1
        out += ["fn main() {", f'    println("{{step{count - 1}(1, 2, true)}}")', "}"]
        print("\n".join(out))
        return
    while len(out) < args.lines:
        out += structure(rng) if rng.randrange(5) == 0 else function(rng)
    print("\n".join(out[:args.lines]))


if __name__ == "__main__":
    main()
