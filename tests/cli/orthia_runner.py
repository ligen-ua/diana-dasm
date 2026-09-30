"""Thin wrapper around the orthia executable in --cmd (headless) mode.

stdout carries only command results; progress and symbol-search noise goes
to stderr, so tests assert on stdout lines and the exit code.
"""
import dataclasses
import os
import re
import subprocess
from pathlib import Path
from typing import Dict, List, Optional

# Exit codes documented in `orthia --help`
EXIT_OK = 0
EXIT_COMMAND_ERROR = 1
EXIT_BAD_ARGUMENT = 2
EXIT_OPEN_FAILED = 3
EXIT_UNEXPECTED = 4

ADDR = r"[0-9a-fA-F]{8}`[0-9a-fA-F]{8}|[0-9a-fA-F]{8}"
_X_LINE = re.compile(rf"^({ADDR})\s+(\S+!\S+)$")
_ADDR_PREFIX = re.compile(rf"^({ADDR})\s")


def parse_addr(text: str) -> int:
    """'00000001`4015dae0' or '4015dae0' -> int."""
    return int(text.replace("`", ""), 16)


@dataclasses.dataclass(repr=False)
class Result:
    argv: List[str]
    code: int
    stdout: str
    stderr: str

    @property
    def lines(self) -> List[str]:
        return [line.rstrip() for line in self.stdout.splitlines() if line.strip()]

    @property
    def errors(self) -> List[str]:
        return [line for line in self.lines if line.startswith("Error:")]

    @property
    def symbols(self) -> Dict[str, List[int]]:
        """Parse `x` output: {'mod!name': [addr, ...]} (a list, so duplicates stay visible)."""
        found: Dict[str, List[int]] = {}
        for line in self.lines:
            m = _X_LINE.match(line)
            if m:
                found.setdefault(m.group(2), []).append(parse_addr(m.group(1)))
        return found

    def first_addr(self) -> int:
        """Address that starts the first stdout line (the start of `u`/`db` output)."""
        for line in self.lines:
            m = _ADDR_PREFIX.match(line)
            if m:
                return parse_addr(m.group(1))
        raise AssertionError(f"no address in output\n{self}")

    def instructions(self) -> List[str]:
        """`u` output lines that are instructions, not '; label' annotations."""
        return [line for line in self.lines
                if _ADDR_PREFIX.match(line) and not re.match(rf"^({ADDR})\s+;", line)]

    def assert_ok(self) -> "Result":
        assert self.code == EXIT_OK and not self.errors, f"expected success\n{self}"
        return self

    def assert_line(self, pattern: str) -> re.Match:
        for line in self.lines:
            m = re.search(pattern, line)
            if m:
                return m
        raise AssertionError(f"no stdout line matches {pattern!r}\n{self}")

    def __str__(self) -> str:
        stderr_tail = "\n".join(self.stderr.splitlines()[-15:])
        return (f"$ {' '.join(self.argv)}\n"
                f"exit code: {self.code}\n"
                f"--- stdout ---\n{self.stdout}"
                f"--- stderr (tail) ---\n{stderr_tail}")

    __repr__ = __str__


class Orthia:
    """A configured orthia binary: one data home (DB cache), one symbol path, and
    whether targets are opened with --analyze (deep analysis + symbols) or quickly."""

    def __init__(self, exe: Path, home: Path, symbol_path: Path, analyze: bool = False,
                 timeout: float = 300):
        self.exe = Path(exe)
        self.home = Path(home)
        self.symbol_path = Path(symbol_path)
        self.analyze = analyze
        self.timeout = timeout

    def with_(self, **changes) -> "Orthia":
        """Same binary with some settings changed, e.g. orthia.with_(analyze=True)."""
        settings = dict(exe=self.exe, home=self.home, symbol_path=self.symbol_path,
                        analyze=self.analyze, timeout=self.timeout)
        settings.update(changes)
        return Orthia(**settings)

    def env(self) -> Dict[str, str]:
        env = dict(os.environ)
        env["ORTHIA_HOME"] = str(self.home)
        env["ORTHIA_SYMBOL_PATH"] = str(self.symbol_path)
        return env

    def raw(self, *args: str, timeout: Optional[float] = None) -> Result:
        """Run with exactly these arguments (for argument-parsing tests)."""
        argv = [str(self.exe), *map(str, args)]
        proc = subprocess.run(argv,
                              capture_output=True,
                              encoding="utf-8",
                              errors="replace",
                              env=self.env(),
                              stdin=subprocess.DEVNULL,
                              timeout=timeout or self.timeout)
        return Result(argv, proc.returncode, proc.stdout, proc.stderr)

    def run(self, *cmds: str, file: Optional[Path] = None, pid: Optional[str] = None,
            timeout: Optional[float] = None) -> Result:
        """Open one target and run each command as its own --cmd."""
        if (file is None) == (pid is None):
            raise ValueError("pass exactly one of file= or pid=")
        args: List[str] = ["--file", str(file)] if file is not None else ["--pid", str(pid)]
        if self.analyze:
            args.append("--analyze")
        for cmd in cmds:
            args += ["--cmd", cmd]
        return self.raw(*args, timeout=timeout)

    def unresolved_module(self, **target) -> str:
        """Name of a dependency that could not be located: `lm` marks it `unresolved`.
        Takes an API set (api-ms-*/ext-ms-*), which is never a file on any host, rather
        than hard-coding one whose presence depends on the Windows version."""
        res = self.run("lm", **target).assert_ok()
        return res.assert_line(r"\s((?:api|ext)-ms-\S+)\s+unresolved\b").group(1)

    def resolve(self, expression: str, **target) -> int:
        """Address an expression evaluates to, via `db <expr> L1` (works even where `u` can't read memory)."""
        res = self.run(f"db {expression} L1", **target)
        res.assert_ok()
        return res.first_addr()
