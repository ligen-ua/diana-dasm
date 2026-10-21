"""Process databases: one folder per process start, reused by a later open of the same process,
replaced with --overwrite-db, and meta.xml in file and process folders."""
import re
import sqlite3
import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.process

REUSED = "Using the database of an earlier open"
# "[pid] name 20261021-153012": the start in UTC
PROC_FOLDER = re.compile(r"^\[(\d+)\] .+ \d{8}-\d{6}$")


@pytest.fixture
def child():
    """A process that runs until its stdin is closed."""
    proc = subprocess.Popen([sys.executable, "-c", "import sys; print('ready', flush=True); sys.stdin.read()"],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        assert proc.stdout.readline().startswith("ready"), "the child didn't start"
        yield proc
    finally:
        proc.stdin.close()
        proc.wait(timeout=30)


def proc_folders(orthia):
    folder = orthia.home / "proc"
    return sorted(folder.iterdir()) if folder.is_dir() else []


def only_folder(orthia) -> Path:
    folders = proc_folders(orthia)
    assert len(folders) == 1, folders
    return folders[0]


def add_comment(folder: Path):
    con = sqlite3.connect(folder / "data.db")
    try:
        con.execute("INSERT INTO tbl_comments(com_address, com_text) VALUES (16, 'kept')")
        con.commit()
    finally:
        con.close()


def comment_count(folder: Path) -> int:
    con = sqlite3.connect(folder / "data.db")
    try:
        return con.execute("SELECT count(*) FROM tbl_comments").fetchone()[0]
    finally:
        con.close()


def meta_attributes(folder: Path) -> dict:
    text = (folder / "meta.xml").read_text(encoding="utf-8")
    return dict(re.findall(r'(\w+)="([^"]*)"', text))


def test_reopen_reuses_database(orthia_cold, child):
    res = orthia_cold.run("lm", pid=str(child.pid)).assert_ok()
    assert REUSED not in res.stderr, res
    folder = only_folder(orthia_cold)
    match = PROC_FOLDER.match(folder.name)
    assert match and int(match.group(1)) == child.pid, folder.name

    meta = meta_attributes(folder)
    assert meta["kind"] == "process", meta
    assert int(meta["pid"], 16) == child.pid, meta
    assert meta["start_key"], meta

    add_comment(folder)
    res = orthia_cold.run("lm", pid=str(child.pid)).assert_ok()
    assert REUSED in res.stderr, res
    assert only_folder(orthia_cold) == folder
    assert comment_count(folder) == 1

    res = orthia_cold.run("lm", pid=str(child.pid), options=("--overwrite-db",)).assert_ok()
    assert REUSED not in res.stderr, res
    assert only_folder(orthia_cold) == folder
    assert comment_count(folder) == 0


def test_reopen_keeps_the_analysis(orthia_cold, child):
    deep = orthia_cold.with_(analyze=True)
    deep.run("lm", pid=str(child.pid)).assert_ok()
    res = deep.run("lm", pid=str(child.pid)).assert_ok()
    assert REUSED in res.stderr, res
    # the main module is not analyzed again: its saved flag is trusted
    assert "Reloading" not in res.stderr, res
    res.assert_line(r"\s+analysis")


def test_folder_of_exited_process_is_cleaned_up(orthia_cold, child):
    orthia_cold.run("lm", pid=str(child.pid)).assert_ok()
    folder = only_folder(orthia_cold)
    child.stdin.close()
    child.wait(timeout=30)
    res = orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    res.assert_line(r"^Removed 1 folder\(s\)")
    assert not folder.exists()


def test_own_process_folder_is_removed_on_close(orthia_cold):
    orthia_cold.run("lm", pid="self").assert_ok()
    assert proc_folders(orthia_cold) == []


def test_file_meta_and_overwrite(orthia_cold, data):
    target = data.self_file
    orthia_cold.run("lm", file=target).assert_ok()
    [folder] = sorted((orthia_cold.home / "db").iterdir())
    meta = meta_attributes(folder)
    assert meta["kind"] == "file", meta
    assert meta["sha1"] == folder.name, meta
    assert meta["format"] in ("pe", "elf"), meta
    assert int(meta["size"], 16) == target.stat().st_size, meta

    add_comment(folder)
    orthia_cold.run("lm", file=target).assert_ok()
    assert comment_count(folder) == 1

    orthia_cold.run("lm", file=target, options=("--overwrite-db",)).assert_ok()
    assert comment_count(folder) == 0
    assert (folder / "target.bin").is_file()


def test_file_database_without_meta_gets_one(orthia_cold, data):
    target = data.self_file
    orthia_cold.run("lm", file=target).assert_ok()
    [folder] = sorted((orthia_cold.home / "db").iterdir())
    (folder / "meta.xml").unlink()
    add_comment(folder)
    orthia_cold.run("lm", file=target).assert_ok()
    assert meta_attributes(folder)["kind"] == "file"
    assert comment_count(folder) == 1


def test_newer_file_database_is_refused(orthia_cold, data):
    target = data.self_file
    orthia_cold.run("lm", file=target).assert_ok()
    [folder] = sorted((orthia_cold.home / "db").iterdir())
    (folder / "meta.xml").write_text('<root format_version="ff" min_reader_version="ff" orthia_version="9.9" kind="file"/>',
                                     encoding="utf-8")
    res = orthia_cold.run("lm", file=target)
    assert res.code != 0, res
    assert "made by a newer Orthia (9.9)" in res.stdout + res.stderr, res

    orthia_cold.run("lm", file=target, options=("--overwrite-db",)).assert_ok()
    assert meta_attributes(folder)["min_reader_version"] == "1"
