#!/usr/bin/env python3
"""Ofir test runner.

Discovers test files under tests/<suite>/, runs them against a compiler and
reports the results. Test formats are described in docs/design/testing.md.

Compiler interface used by the runner:

    ofir build FILE --emit=tokens          print tokens to stdout
    ofir build FILE --emit=ast             print the AST to stdout
    ofir build FILE --emit=llvm -o OUT     write OUT.ll
    ofir build FILE --message-format=short diagnostics as
                                            `file:line:col: error[E0300]: message`
    ofir run FILE                          compile and run

Standard library only; Python 3.9 or newer.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import os
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path

SUITES = ["lex", "parse", "run", "compile_fail", "panic", "borrow", "codegen", "fmt", "doc",
          "lsp"]
TIMEOUT_SECONDS = 20
PANIC_EXIT_STATUS = 101

ANNOTATION_RE = re.compile(r"//\s*(error|warning)\[([EW]\d{4})\]:\s*(.*?)\s*$")
DIAGNOSTIC_RE = re.compile(r"^(.*?):(\d+):(\d+): (error|warning)\[([EW]\d{4})\]: (.*)$")
REQUIRES_RE = re.compile(r"^//\s*requires:\s*(\S+)", re.MULTILINE)
# Extra compiler flags of a run or codegen test, e.g. `// flags: --release`.
FLAGS_RE = re.compile(r"^//\s*flags:\s*(.+)$", re.MULTILINE)
PANIC_RE = re.compile(r"^//\s*panic:\s*(.*?)\s*$", re.MULTILINE)
CHECK_RE = re.compile(r"^//\s*(CHECK|CHECK-NOT):\s*(.*?)\s*$")
TOP_LEVEL_FN_RE = re.compile(r"^(pub\s+)?(unsafe\s+)?fn\b")


@dataclass
class Result:
    path: Path
    status: str  # "pass", "fail" or "skip"
    detail: str = ""


@dataclass
class Diagnostic:
    line: int
    severity: str
    code: str
    message: str


@dataclass
class Config:
    compiler: str
    stage: int
    bless: bool
    tests_dir: Path
    verbose: bool
    tmp: Path = field(default_factory=lambda: Path(tempfile.mkdtemp(prefix="ofir-tests-")))


def executable(path: str) -> str:
    """The compiler as an absolute path; on Windows, `.exe` may be implied."""
    full = Path(path).resolve()
    if os.name == "nt" and not full.suffix and full.with_suffix(".exe").exists():
        full = full.with_suffix(".exe")
    return str(full)


def run_process(cmd: list[str], cwd: Path | None = None,
                input_text: str | None = None) -> subprocess.CompletedProcess:
    try:
        return subprocess.run(
            cmd, cwd=cwd, capture_output=True, text=True, timeout=TIMEOUT_SECONDS,
            input=input_text
        )
    except subprocess.TimeoutExpired as exc:
        message = f"timeout after {TIMEOUT_SECONDS}s"
        return subprocess.CompletedProcess(cmd, -1, exc.stdout or "", message)


def compiler_cmd(cfg: Config, *args: str) -> list[str]:
    # A compiler given as a .py file (used by the runner self-test) runs
    # through the current interpreter.
    if cfg.compiler.endswith(".py"):
        return [sys.executable, cfg.compiler, *args]
    return [cfg.compiler, *args]


def describe(proc: subprocess.CompletedProcess) -> str:
    return (f"exit status {proc.returncode}\n"
            f"--- stdout ---\n{proc.stdout}--- stderr ---\n{proc.stderr}")


def compare_output(cfg: Config, path: Path, actual: str, expected_suffix: str) -> Result:
    expected_path = path.with_suffix(expected_suffix)
    if cfg.bless:
        expected_path.write_text(actual)
        return Result(path, "pass", "blessed")
    if not expected_path.exists():
        return Result(path, "fail", f"missing {expected_path.name} (run with --bless to create it)")
    expected = expected_path.read_text()
    if actual == expected:
        return Result(path, "pass")
    path.with_suffix(".actual").write_text(actual)
    return Result(path, "fail", f"output differs from {expected_path.name}; actual output saved to "
                                f"{path.with_suffix('.actual').name}")


def parse_annotations(source: str) -> list[tuple[int, Diagnostic]]:
    """Returns (function_start_line, diagnostic) for each annotation."""
    annotations = []
    current_fn = 0
    for number, text in enumerate(source.splitlines(), start=1):
        if TOP_LEVEL_FN_RE.match(text):
            current_fn = number
        m = ANNOTATION_RE.search(text)
        if m:
            annotations.append((current_fn, Diagnostic(number, m.group(1), m.group(2), m.group(3))))
    return annotations


def parse_diagnostics(output: str) -> list[Diagnostic]:
    diagnostics = []
    for text in output.splitlines():
        m = DIAGNOSTIC_RE.match(text)
        if m:
            diagnostics.append(Diagnostic(int(m.group(2)), m.group(4), m.group(5), m.group(6)))
    return diagnostics


def unexpected_diagnostics(reported, annotations) -> list[str]:
    """Every reported error must be annotated (both stages); every warning
    too, in a test that annotates warnings. A test of errors is not about
    the unused variables it needs to trigger them."""
    annotated = {(d.line, d.code) for _, d in annotations}
    checks_warnings = any(d.severity == "warning" for _, d in annotations)
    return [f"unexpected {d.severity}[{d.code}] at line {d.line}: {d.message}"
            for d in reported if (d.line, d.code) not in annotated
            and (d.severity == "error" or checks_warnings)]


def missing_stage0(reported, annotations) -> list[str]:
    """Stage 0 reports only the first error of each function and its messages
    are terse: require the first annotation of each function."""
    reported_keys = {(d.line, d.code) for d in reported}
    firsts: dict[int, Diagnostic] = {}
    for fn_line, d in annotations:
        firsts.setdefault(fn_line, d)
    return [f"missing {d.severity}[{d.code}] at line {d.line}"
            for d in firsts.values() if (d.line, d.code) not in reported_keys]


def missing_stage1(reported, annotations) -> list[str]:
    """Stage 1 must report every annotation, with a matching message."""
    problems = []
    for _, d in annotations:
        matches = [r for r in reported if (r.line, r.code) == (d.line, d.code)]
        if not matches:
            problems.append(f"missing {d.severity}[{d.code}] at line {d.line}: {d.message}")
        elif d.message and not any(d.message in r.message for r in matches):
            problems.append(f"message mismatch at line {d.line}: expected '{d.message}', "
                            f"got '{matches[0].message}'")
    return problems


def check_diagnostics(cfg: Config, path: Path, source: str) -> Result:
    annotations = parse_annotations(source)
    proc = run_process(compiler_cmd(cfg, "build", str(path), "--message-format=short",
                                    "-o", str(cfg.tmp / path.stem)))
    reported = parse_diagnostics(proc.stdout + proc.stderr)
    expects_error = any(d.severity == "error" for _, d in annotations)

    if expects_error and proc.returncode == 0:
        return Result(path, "fail", "expected compilation to fail\n" + describe(proc))
    if not expects_error and proc.returncode != 0:
        return Result(path, "fail", "expected compilation to succeed\n" + describe(proc))

    missing = missing_stage0 if cfg.stage == 0 else missing_stage1
    problems = unexpected_diagnostics(reported, annotations) + missing(reported, annotations)
    if problems:
        return Result(path, "fail", "\n".join(problems))
    return Result(path, "pass")


def check_panic(cfg: Config, path: Path, source: str) -> Result:
    m = PANIC_RE.search(source)
    if not m:
        return Result(path, "fail", "missing `// panic: message` header")
    proc = run_process(compiler_cmd(cfg, "run", str(path)))
    if proc.returncode != PANIC_EXIT_STATUS:
        return Result(path, "fail", f"expected exit status {PANIC_EXIT_STATUS}\n" + describe(proc))
    if f"panic: {m.group(1)}" not in proc.stderr:
        return Result(path, "fail", f"expected 'panic: {m.group(1)}' on stderr\n" + describe(proc))
    return Result(path, "pass")


def test_flags(source: str) -> list[str]:
    return [flag for line in FLAGS_RE.findall(source) for flag in line.split()]


def check_run(cfg: Config, path: Path, source: str) -> Result:
    # Programs that run are warning-free: a warning fails the test.
    proc = run_process(compiler_cmd(cfg, "run", str(path), "--deny-warnings",
                                    *test_flags(source)))
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    return compare_output(cfg, path, proc.stdout, ".out")


def check_emit(cfg: Config, path: Path, what: str, suffix: str) -> Result:
    proc = run_process(compiler_cmd(cfg, "build", str(path), f"--emit={what}"))
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    return compare_output(cfg, path, proc.stdout, suffix)


def check_codegen(cfg: Config, path: Path, source: str) -> Result:
    out = cfg.tmp / path.stem
    proc = run_process(compiler_cmd(cfg, "build", str(path), "--emit=llvm", "-o", str(out),
                                    *test_flags(source)))
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    ir_path = out.with_suffix(".ll")
    if not ir_path.exists():
        return Result(path, "fail", f"compiler did not write {ir_path}")
    ir_lines = ir_path.read_text().splitlines()

    # A small subset of LLVM FileCheck: CHECK patterns must appear in order;
    # CHECK-NOT patterns must not appear between the surrounding CHECK matches.
    position = 0
    pending_not: list[str] = []
    checks = [CHECK_RE.match(line) for line in source.splitlines()]
    for m in filter(None, checks):
        kind, pattern = m.group(1), m.group(2)
        if kind == "CHECK-NOT":
            pending_not.append(pattern)
            continue
        found = next((i for i in range(position, len(ir_lines)) if pattern in ir_lines[i]), None)
        if found is None:
            return Result(path, "fail", f"CHECK not found: {pattern}")
        for banned in pending_not:
            if any(banned in line for line in ir_lines[position:found]):
                return Result(path, "fail", f"CHECK-NOT matched: {banned}")
        pending_not.clear()
        position = found + 1
    for banned in pending_not:
        if any(banned in line for line in ir_lines[position:]):
            return Result(path, "fail", f"CHECK-NOT matched: {banned}")
    return Result(path, "pass")


def check_fmt(cfg: Config, path: Path) -> Result:
    """`ofir fmt --stdout` of the file must match its .fmt file, and
    formatting that output again must not change it."""
    if cfg.stage == 0:
        return Result(path, "skip", "`fmt` is a stage 1 command")
    proc = run_process(compiler_cmd(cfg, "fmt", "--stdout", str(path)))
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    result = compare_output(cfg, path, proc.stdout, ".fmt")
    if result.status != "pass" or cfg.bless:
        return result
    with tempfile.TemporaryDirectory() as tmp:
        again_path = Path(tmp) / path.name
        again_path.write_text(proc.stdout)
        again = run_process(compiler_cmd(cfg, "fmt", "--stdout", str(again_path)))
    if again.returncode != 0 or again.stdout != proc.stdout:
        return Result(path, "fail", "formatting the output again changes it\n" + describe(again))
    return result


def check_doc(cfg: Config, path: Path) -> Result:
    """`ofir doc --stdout` of the file must match its .html file."""
    if cfg.stage == 0:
        return Result(path, "skip", "`doc` is a stage 1 command")
    proc = run_process(compiler_cmd(cfg, "doc", "--stdout", str(path)))
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    return compare_output(cfg, path, proc.stdout, ".html")


# The session of an `lsp` test without a `.lsp` file: open the document.
DEFAULT_LSP_SCRIPT = """\
{"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"capabilities": {}}}
{"jsonrpc": "2.0", "method": "initialized", "params": {}}
{"jsonrpc": "2.0", "method": "textDocument/didOpen", "params": {"textDocument": \
{"uri": "$URI", "languageId": "ofir", "version": 1, "text": $TEXT}}}
{"jsonrpc": "2.0", "id": 2, "method": "shutdown"}
{"jsonrpc": "2.0", "method": "exit"}
"""


def check_lsp(cfg: Config, path: Path) -> Result:
    """`ofir lsp` is sent the messages of the test's `.lsp` file, one JSON
    object per line, where `$URI` is the `.of` file's URI and `$TEXT` its
    text as a JSON string; its messages, one per line with the URI written
    `$URI` and the standard library's directory `$STD`, must match the `.out`
    file, and it must exit with status 0."""
    if cfg.stage == 0:
        return Result(path, "skip", "`lsp` is a stage 1 command")
    script_path = path.with_suffix(".lsp")
    script = script_path.read_text() if script_path.exists() else DEFAULT_LSP_SCRIPT
    uri = path.resolve().as_uri()
    std = Path(os.environ.get("OFIR_STD", Path(__file__).resolve().parents[2] / "std"))
    std_uri = std.resolve().as_uri()
    text = json.dumps(path.read_text())
    framed = ""
    for line in script.replace("\\\n", "").splitlines():
        if line.strip():
            body = line.replace("$URI", uri).replace("$TEXT", text)
            framed += f"Content-Length: {len(body.encode())}\r\n\r\n{body}"
    proc = run_process(compiler_cmd(cfg, "lsp"), input_text=framed)
    if proc.returncode != 0:
        return Result(path, "fail", describe(proc))
    messages = []
    # Text mode reads each `\r\n` of the headers as `\n`; the bodies, JSON
    # written on one line, are unchanged.
    rest = proc.stdout.encode()
    while rest:
        header, _, rest = rest.partition(b"\n\n")
        length = int(header.split(b":")[1])
        messages.append(rest[:length].decode().replace(uri, "$URI").replace(std_uri, "$STD"))
        rest = rest[length:]
    return compare_output(cfg, path, "".join(m + "\n" for m in messages), ".out")


def run_test(cfg: Config, suite: str, path: Path) -> Result:
    source = path.read_text()
    for requirement in REQUIRES_RE.findall(source):
        if requirement == "stage1" and cfg.stage == 0:
            return Result(path, "skip", "requires stage1")
        if requirement == "stage0" and cfg.stage == 1:
            return Result(path, "skip", "requires stage0")
    if suite == "lex":
        return check_emit(cfg, path, "tokens", ".tokens")
    if suite == "parse":
        return check_emit(cfg, path, "ast", ".ast")
    if suite == "run":
        return check_run(cfg, path, source)
    if suite == "panic":
        return check_panic(cfg, path, source)
    if suite == "codegen":
        return check_codegen(cfg, path, source)
    if suite == "fmt":
        return check_fmt(cfg, path)
    if suite == "doc":
        return check_doc(cfg, path)
    if suite == "lsp":
        return check_lsp(cfg, path)
    # compile_fail and borrow
    return check_diagnostics(cfg, path, source)


def is_module_of_project(path: Path, suite_dir: Path) -> bool:
    """A directory with a main.of is one multi-file test: its other files are
    modules of that program, not tests."""
    for parent in path.parents:
        if parent == suite_dir or suite_dir not in parent.parents:
            break
        if (parent / "main.of").exists() and path != parent / "main.of":
            return True
    return False


def discover(tests_dir: Path, filters: list[str]) -> list[tuple[str, Path]]:
    found = []
    for suite in SUITES:
        suite_dir = tests_dir / suite
        for path in sorted(suite_dir.rglob("*.of")):
            if is_module_of_project(path, suite_dir):
                continue
            rel = path.relative_to(tests_dir).as_posix()
            if not filters or any(f in rel for f in filters):
                found.append((suite, path))
    return found


def main(argv: list[str]) -> int:
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("filters", nargs="*",
                        help="run only tests whose path contains one of these")
    parser.add_argument("--compiler", type=executable,
                        default=str(root / "build" / "stage0" / "ofir"))
    parser.add_argument("--stage", type=int, choices=[0, 1],
                        help="compiler stage (default: 0 if the path contains 'stage0')")
    parser.add_argument("--tests-dir", type=Path, default=root / "tests")
    parser.add_argument("--bless", action="store_true", help="overwrite expected output files")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 1)
    parser.add_argument("-v", "--verbose", action="store_true", help="also list passing tests")
    args = parser.parse_args(argv)

    stage = args.stage if args.stage is not None else (0 if "stage0" in args.compiler else 1)
    cfg = Config(args.compiler, stage, args.bless, args.tests_dir.resolve(), args.verbose)

    tests = discover(cfg.tests_dir, args.filters)
    if not tests:
        print("no tests found")
        return 0
    if not Path(cfg.compiler).exists():
        print(f"error: compiler not found: {cfg.compiler}", file=sys.stderr)
        return 2

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda t: run_test(cfg, *t), tests))

    counts = {"pass": 0, "fail": 0, "skip": 0}
    for r in results:
        counts[r.status] += 1
        rel = r.path.relative_to(cfg.tests_dir).as_posix()
        if r.status == "fail":
            print(f"FAIL {rel}\n" + "\n".join("     " + line for line in r.detail.splitlines()))
        elif cfg.verbose:
            print(f"{r.status.upper():4} {rel}" + (f" ({r.detail})" if r.detail else ""))
    print(f"\n{len(results)} tests: {counts['pass']} passed, {counts['fail']} failed, "
          f"{counts['skip']} skipped (stage {stage})")
    return 1 if counts["fail"] else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
