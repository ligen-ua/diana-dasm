"""Argument parsing and exit codes (report section "Argument parsing")."""
import os
import re

import pytest

from data import REPO_ROOT
from orthia_runner import (EXIT_BAD_ARGUMENT, EXIT_COMMAND_ERROR, EXIT_OK,
                           EXIT_OPEN_FAILED)


@pytest.fixture(scope="module")
def target(data):
    """The binary under test as a file: the target here doesn't matter, only that it opens."""
    return data.self_file


@pytest.mark.parametrize("flag", ["--help", "-h"])
def test_help(orthia, flag):
    res = orthia.raw(flag)
    assert res.code == EXIT_OK, res
    for option in ("--file", "--pid", "--cmd", "--analyze", "Exit codes", "ORTHIA_HOME", "ORTHIA_SYMBOL_PATH"):
        assert option in res.stdout, res


def test_version(orthia):
    header = (REPO_ROOT / "src/orthia/orthia_disasm_ui/orthia_version.h").read_text()
    parts = [re.search(rf'#define ORTHIA_UI_VER_{name}_STR\s+"(\d+)"', header).group(1)
             for name in ("MAJOR", "MINOR", "PATCH", "REVISION")]
    res = orthia.raw("--version")
    assert res.code == EXIT_OK, res
    assert res.stdout == f"orthia {'.'.join(parts)}\n", res
    assert "--version" in orthia.raw("--help").stdout


def test_missing_file(orthia, tmp_path):
    res = orthia.run("lm", file=tmp_path / "nonexist.exe")
    assert res.code == EXIT_OPEN_FAILED, res


def test_directory_as_file(orthia, tmp_path):
    res = orthia.run("lm", file=tmp_path)
    assert res.code == EXIT_OPEN_FAILED, res


def test_empty_file(orthia, data):
    res = orthia.run("lm", file=data.text_file("empty.bin", b""))
    assert res.code == EXIT_OPEN_FAILED, res


def test_non_executable_file(orthia, data):
    res = orthia.run("lm", file=data.text_file("text.txt", b"just some text\n" * 100))
    assert res.code == EXIT_OPEN_FAILED, res


def test_cmd_without_target(orthia):
    assert orthia.raw("--cmd", "lm").code == EXIT_OPEN_FAILED


@pytest.mark.parametrize("args", [
    ["--bogus"],
    ["--file"],
    ["--pid"],
    ["--cmd"],
])
def test_bad_arguments(orthia, args):
    res = orthia.raw(*args)
    assert res.code == EXIT_BAD_ARGUMENT, res


def test_file_and_pid_together(orthia, target):
    res = orthia.raw("--file", str(target), "--pid", "self", "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res


def test_unknown_command(orthia, target):
    res = orthia.run("nosuchcommand", file=target)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Command not found: nosuchcommand")


def test_error_does_not_stop_later_commands(orthia, target):
    res = orthia.run("nosuchcommand", "lm", file=target)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(rf"\b{re.escape(target.name)}\b")


def test_empty_command_is_noop(orthia, target):
    assert orthia.run("", file=target).code == EXIT_OK


def test_whitespace_command_is_noop(orthia, target):
    orthia.run("   ", file=target).assert_ok()


def test_cmd_error_echoes_whole_token(orthia, target):
    res = orthia.run("--file", file=target)
    res.assert_line(r"Command not found: --file")


@pytest.mark.parametrize("pid", ["abc", "12abc"])
def test_non_numeric_pid(orthia, pid):
    res = orthia.raw("--pid", pid, "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res


def test_negative_pid(orthia):
    res = orthia.raw("--pid", "-1", "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res


@pytest.mark.process
def test_hex_pid(orthia):
    # the test runner's own process: hex should open it, or be rejected as a bad argument
    res = orthia.raw("--pid", hex(os.getpid()), "--cmd", "lm")
    if res.code == EXIT_OK:
        res.assert_line(r"(?i)\bpython")
    else:
        assert res.code == EXIT_BAD_ARGUMENT, res


def test_ui_mode_without_console_fails(orthia_cold, target):
    # stdin/stdout are not a console here, as in a script or CI. orthia_cold: the UI does a full open
    res = orthia_cold.raw("--file", str(target), timeout=30)
    assert res.code != EXIT_OK, res


def test_missing_file_reports_reason(orthia, tmp_path):
    res = orthia.run("lm", file=tmp_path / "nonexist.exe")
    assert res.code == EXIT_OPEN_FAILED, res
    assert re.search(r"(?i)cannot find|no such file", res.stderr), res


def test_directory_as_file_reports_reason(orthia, tmp_path):
    res = orthia.run("lm", file=tmp_path)
    assert res.code == EXIT_OPEN_FAILED, res
    assert re.search(r"(?i)access is denied|is a directory", res.stderr), res
