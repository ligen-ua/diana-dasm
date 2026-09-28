"""Argument parsing and exit codes (report section "Argument parsing")."""
import pytest

from orthia_runner import (EXIT_BAD_ARGUMENT, EXIT_COMMAND_ERROR, EXIT_OK,
                           EXIT_OPEN_FAILED)


@pytest.fixture(scope="module")
def dmesg(data):
    return data.elf("dmesg")


@pytest.mark.parametrize("flag", ["--help", "-h"])
def test_help(orthia, flag):
    res = orthia.raw(flag)
    assert res.code == EXIT_OK, res
    for option in ("--file", "--pid", "--cmd", "--analyze", "Exit codes", "ORTHIA_HOME", "ORTHIA_SYMBOL_PATH"):
        assert option in res.stdout, res


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


def test_file_and_pid_together(orthia, dmesg):
    res = orthia.raw("--file", str(dmesg), "--pid", "self", "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res


def test_unknown_command(orthia, dmesg):
    res = orthia.run("nosuchcommand", file=dmesg)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Command not found: nosuchcommand")


def test_error_does_not_stop_later_commands(orthia, dmesg):
    res = orthia.run("nosuchcommand", "lm", file=dmesg)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"\bdmesg\b")


def test_empty_command_is_noop(orthia, dmesg):
    assert orthia.run("", file=dmesg).code == EXIT_OK


@pytest.mark.xfail(reason="B13: whitespace-only --cmd reports 'Command not found'")
def test_whitespace_command_is_noop(orthia, dmesg):
    orthia.run("   ", file=dmesg).assert_ok()


@pytest.mark.xfail(reason="B14: error shows a truncated token '-'")
def test_cmd_error_echoes_whole_token(orthia, dmesg):
    res = orthia.run("--file", file=dmesg)
    res.assert_line(r"Command not found: --file")


@pytest.mark.parametrize("pid", ["abc", "12abc"])
@pytest.mark.xfail(reason="B6: non-numeric --pid exits 4 (unexpected) instead of 2")
def test_non_numeric_pid(orthia, pid):
    res = orthia.raw("--pid", pid, "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res
