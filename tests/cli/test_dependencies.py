"""Dependencies in file mode: linked from disk, checked by identity, stale when the file changes.

ntoskrnl imports bootvid.dll, which every Windows host has with real (non-forwarded) exports,
so the host DLL tests run on Windows only. The ELF test needs a Linux host with libc.so.6.
"""
import re
import shutil
import sqlite3
import sys
from pathlib import Path

import pytest

from data import flip_rsds_guid, iat_slot, patch_pe_timestamp
from orthia_runner import EXIT_COMMAND_ERROR

pytestmark = pytest.mark.pe

windows_only = pytest.mark.skipif(sys.platform != "win32", reason="host DLL")
linux_only = pytest.mark.skipif(sys.platform != "linux", reason="host shared object")

DEP = "bootvid.dll"
DEP_FUNC = "VidBitBlt"


@pytest.fixture(scope="module")
def nt(data):
    return {"file": data.ntoskrnl}


@windows_only
def test_lm_shows_linked_dependencies(orthia, nt):
    res = orthia.run("lm", **nt).assert_ok()
    res.assert_line(rf"\b{DEP}\s+linked$")
    res.assert_line(r"\bhal\.dll\s+linked$")
    # an API set is a file on no host: still unresolved
    res.assert_line(r"\s(?:api|ext)-ms-\S+\s+unresolved$")


@windows_only
def test_db_dependency_header(orthia, nt):
    res = orthia.run(f"db {DEP} L2", **nt).assert_ok()
    res.assert_line(r"^00000001`c0010000  4d 5a\s+MZ$")


@windows_only
def test_u_dependency_export(orthia, nt):
    export = orthia.run(f"x {DEP}!{DEP_FUNC}", **nt).assert_ok().symbols[f"{DEP}!{DEP_FUNC}"][0]
    res = orthia.run(f"u {DEP}!{DEP_FUNC} L2", **nt).assert_ok()
    assert res.first_addr() == export, res
    assert len(res.instructions()) == 2, res


@windows_only
def test_modinfo_dependency(orthia, nt):
    res = orthia.run(f"modinfo {DEP}", **nt).assert_ok()
    res.assert_line(r"^Image: linked")
    guid = res.assert_line(r"^Debug GUID: (\S+)$").group(1)
    assert guid.strip("0-"), res
    res.assert_line(r"^Pdb name: bootvid\.pdb$")


@windows_only
def test_analyze_dependency(orthia_cold, nt):
    res = orthia_cold.run(f".analyze {DEP}", "lm", **nt).assert_ok()
    res.assert_line(rf"\b{DEP}\s+linked, analysis$")


@windows_only
def test_iat_slot_is_linked(orthia, nt, data):
    # the import table of the main module holds the dependency's export address, on a warm
    # reopen too: the first open links it, later opens replay the recorded slots
    slot = iat_slot(data.ntoskrnl, DEP, DEP_FUNC)
    export = orthia.run(f"x {DEP}!{DEP_FUNC}", **nt).assert_ok().symbols[f"{DEP}!{DEP_FUNC}"][0]
    res = orthia.run(f"dps {slot:x} L1", **nt).assert_ok()
    line = res.assert_line(rf"^00000001`{slot & 0xFFFFFFFF:08x}\s+(\S+)\s+{DEP}!{DEP_FUNC}$")
    assert int(line.group(1).replace("`", ""), 16) == export, res


@pytest.fixture
def target(data, tmp_path):
    """A fresh copy of ntoskrnl with its own copy of bootvid.dll beside it."""
    exe, dep = data.copy_with_dependency(data.ntoskrnl, DEP, tmp_path / "target")
    return exe, dep


def _dependency_is(res, status):
    res.assert_line(rf"\b{DEP}\s+{status}$")


@windows_only
def test_co_located_dependency_wins(orthia_cold, target):
    exe, dep = target
    res = orthia_cold.run("lm", f"modinfo {DEP}", file=exe).assert_ok()
    _dependency_is(res, "linked")
    full_name = res.assert_line(r"^Full name: (.+)$").group(1)
    assert Path(full_name).resolve() == dep.resolve(), res


@windows_only
def test_stale_after_timestamp_change(orthia_cold, target):
    exe, dep = target
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), "linked")

    patch_pe_timestamp(dep)
    res = orthia_cold.run("lm", f"db {DEP} L2", f"modinfo {DEP}", f"x {DEP}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, "stale")
    res.assert_line(r"^00000001`c0010000  \?\? \?\?")
    res.assert_line(r"^Image: stale \(timestamp mismatch\)$")
    assert f"No image data for module: {DEP}" in res.stdout, res
    # the names recorded at first open survive
    assert f"{DEP}!{DEP_FUNC}" in res.symbols, res


@windows_only
def test_stale_after_guid_change(orthia_cold, target):
    exe, dep = target
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), "linked")

    # the headers still match, so the file is only found out when it is mapped
    flip_rsds_guid(dep)
    res = orthia_cold.run("lm", f"db {DEP} L2", "lm", file=exe)
    rows = re.findall(rf"\b{DEP}\s+(\w+)$", res.stdout, re.M)
    assert rows == ["linked", "stale"], res
    res.assert_line(r"^00000001`c0010000  \?\? \?\?")


@windows_only
def test_stale_after_file_removed(orthia_cold, target):
    exe, dep = target
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), "linked")

    dep.unlink()
    res = orthia_cold.run("lm", f"modinfo {DEP}", f"x {DEP}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, "stale")
    res.assert_line(r"^Image: stale \(file not found\)$")
    assert f"{DEP}!{DEP_FUNC}" in res.symbols, res


@windows_only
def test_old_database_dependency_is_unlinked(orthia_cold, target):
    # a database written before image sources existed has no src_kind: names only
    exe, dep = target
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), "linked")

    (db,) = list(orthia_cold.home.rglob("data.db"))
    con = sqlite3.connect(db)
    try:
        changed = con.execute("UPDATE tbl_metainfo SET meta_info = replace(meta_info, ' src_kind=\"linked\"', '') "
                              "WHERE meta_type = 1 AND meta_info LIKE '%src_kind=\"linked\"%'").rowcount
        con.commit()
    finally:
        con.close()
    assert changed > 0

    res = orthia_cold.run("lm", f"modinfo {DEP}", f"x {DEP}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, "unlinked")
    res.assert_line(r"^Image: none \(recorded without identity\)$")
    assert f"{DEP}!{DEP_FUNC}" in res.symbols, res


@linux_only
@pytest.mark.elf
def test_elf_dependency_linked(orthia_cold, data, tmp_path):
    libc = next((p for p in [Path("/lib/x86_64-linux-gnu/libc.so.6"), Path("/usr/lib64/libc.so.6"),
                             Path("/lib64/libc.so.6")] if p.is_file()), None)
    if libc is None:
        pytest.skip("no libc.so.6 in a known place")
    target = tmp_path / "target"
    target.mkdir()
    exe = target / "dmesg"
    shutil.copyfile(data.elf("dmesg"), exe)
    shutil.copyfile(libc, target / "libc.so.6")

    res = orthia_cold.run("lm", "db libc.so.6 L4", file=exe).assert_ok()
    res.assert_line(r"\blibc\.so\.6\s+linked$")
    res.assert_line(r"  7f 45 4c 46\s+\.ELF$")
