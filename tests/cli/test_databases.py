"""`.database list | delete | cleanup`: the databases in the data folder, run without a target."""
import os
import re
import sys
import time
from pathlib import Path

import pytest

from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OK, EXIT_OPEN_FAILED


@pytest.fixture(scope="module")
def dmesg(data):
    return data.elf("dmesg")


def db_folders(orthia):
    return sorted(p.name for p in (orthia.home / "db").iterdir())


def proc_folders(orthia):
    return sorted(p.name for p in (orthia.home / "proc").iterdir())


def opened(orthia, target):
    """Open target once so it gets a database; returns its sha1 folder name."""
    orthia.run("lm", file=target).assert_ok()
    folders = db_folders(orthia)
    assert len(folders) == 1, folders
    return folders[0]


def make_folder(path: Path, age_hours: float = 0):
    path.mkdir(parents=True)
    if age_hours:
        stamp = time.time() - age_hours * 3600
        os.utime(path, (stamp, stamp))
    return path


def own_process_name():
    """The name the process provider gives this process: its image name / comm.
    Not sys.executable: a venv or Store launcher runs the interpreter under another name."""
    if sys.platform == "win32":
        import ctypes
        from ctypes import wintypes
        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel32.GetCurrentProcess.restype = wintypes.HANDLE
        kernel32.QueryFullProcessImageNameW.argtypes = [
            wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
        buffer = ctypes.create_unicode_buffer(32768)
        size = wintypes.DWORD(len(buffer))
        assert kernel32.QueryFullProcessImageNameW(kernel32.GetCurrentProcess(), 0, buffer, ctypes.byref(size))
        return Path(buffer.value).name
    return Path("/proc/self/comm").read_text().strip()


def test_list_empty(orthia_cold):
    res = orthia_cold.raw("--cmd", ".database list").assert_ok()
    res.assert_line(r"^Data folder: ")
    res.assert_line(r"^No databases$")


def test_list_shows_opened_file(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", ".database list").assert_ok()
    line = res.assert_line(rf"^{sha1[:12]}\s").string
    assert str(dmesg) in line, res
    assert re.search(r"\bquick\b", line), res
    assert "(open)" not in line, res


def test_list_marks_open_item(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.run(".database list", file=dmesg).assert_ok()
    assert res.assert_line(rf"^{sha1[:12]}\s").string.endswith("(open)"), res


def test_list_counts_comments(orthia_cold, dmesg):
    import sqlite3
    sha1 = opened(orthia_cold, dmesg)
    con = sqlite3.connect(orthia_cold.home / "db" / sha1 / "data.db")
    try:
        con.execute("INSERT INTO tbl_comments(com_address, com_text) VALUES (16, 'x'), (32, 'y')")
        con.commit()
    finally:
        con.close()
    res = orthia_cold.raw("--cmd", ".database list").assert_ok()
    assert re.search(r"\s2\s", res.assert_line(rf"^{sha1[:12]}\s").string), res


def test_delete_by_prefix(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", f".database delete {sha1[:8].upper()}").assert_ok()
    res.assert_line(rf"^Deleted {sha1}\b")
    assert db_folders(orthia_cold) == []


def test_delete_by_file_name(orthia_cold, dmesg):
    opened(orthia_cold, dmesg)
    orthia_cold.raw("--cmd", f'.database delete "{dmesg}"').assert_ok()
    assert db_folders(orthia_cold) == []


def test_delete_by_pid(orthia_cold):
    make_folder(orthia_cold.home / "proc" / "[4242] tool.exe")
    orthia_cold.raw("--cmd", ".database delete 4242").assert_ok()
    assert proc_folders(orthia_cold) == []


@pytest.mark.parametrize("selector, message", [
    ("ffffff", r"No database matches: ffffff"),
    ("abc", r"at least 6 hex digits"),
    ("no-such-file.bin", r"Can't open file"),
])
def test_delete_bad_selector(orthia_cold, dmesg, selector, message):
    opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", f".database delete {selector}")
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(rf"^Error: .*{message}")
    assert len(db_folders(orthia_cold)) == 1


def test_delete_file_without_database(orthia_cold, dmesg, data):
    opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", f".database delete {data.elf('apt-mark')}")
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: No database for file")


def test_delete_ambiguous_prefix(orthia_cold):
    for name in ("abcdef01" + "0" * 32, "abcdef02" + "0" * 32):
        make_folder(orthia_cold.home / "db" / name)
    res = orthia_cold.raw("--cmd", ".database delete abcdef")
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Ambiguous: abcdef matches ")
    assert len(db_folders(orthia_cold)) == 2


def test_delete_is_all_or_nothing(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", f".database delete {sha1[:8]} ffffff")
    assert res.code == EXIT_COMMAND_ERROR, res
    assert db_folders(orthia_cold) == [sha1]


def test_delete_open_item_is_refused(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.run(f".database delete {sha1[:8]}", file=dmesg)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Database \w+ is open as dmesg, close it first")
    assert db_folders(orthia_cold) == [sha1]


def test_cleanup(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    proc = orthia_cold.home / "proc"
    db = orthia_cold.home / "db"
    live = f"[{os.getpid()}] {own_process_name()}"
    make_folder(proc / live)                                    # kept: the process runs
    make_folder(proc / f"[{os.getpid()}] other-program.exe")    # removed: pid now runs another program
    make_folder(proc / "[4000000000] gone.exe")                 # removed: no such process
    make_folder(proc / "no-pid")                                # kept: can't tell
    make_folder(db / ("0" * 40), age_hours=2)                   # removed: no data.db
    make_folder(db / ("1" * 40))                                # kept: may be being created
    make_folder(db / ("2" * 40 + ".deleting"))                  # removed: unfinished delete

    res = orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    res.assert_line(r"^Removed 4 folder\(s\)")
    assert proc_folders(orthia_cold) == sorted([live, "no-pid"])
    assert db_folders(orthia_cold) == sorted(["1" * 40, sha1])

    res = orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    res.assert_line(r"^Nothing to clean up$")


def test_cleanup_expired_process_folder(orthia_cold):
    live = f"[{os.getpid()}] {own_process_name()}"
    make_folder(orthia_cold.home / "proc" / live, age_hours=49)
    orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    assert proc_folders(orthia_cold) == []


@pytest.mark.parametrize("cmd", [".database", ".database list extra", ".database frobnicate"])
def test_usage(orthia_cold, cmd):
    res = orthia_cold.raw("--cmd", cmd)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Usage: \.database")


def test_item_commands_still_need_a_target(orthia_cold):
    res = orthia_cold.raw("--cmd", ".database list", "--cmd", "lm")
    assert res.code == EXIT_OPEN_FAILED, res
    assert res.lines == [], res


def test_several_targetless_commands(orthia_cold, dmesg):
    sha1 = opened(orthia_cold, dmesg)
    res = orthia_cold.raw("--cmd", f".database delete {sha1[:8]}", "--cmd", ".database list")
    assert res.code == EXIT_OK, res
    res.assert_line(r"^No databases$")
