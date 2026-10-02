"""Dependencies in file mode on the binary under test: the counterpart of test_dependencies.py.

orthia.exe imports a few System32 DLLs; data.host_import picks one imported function whose host
export is real code (not a forwarder). The ELF case (libc beside the exe) is in test_elf_self.py.
"""
import re
import sqlite3
from pathlib import Path

import pytest

from data import flip_rsds_guid, host_import, iat_slot, patch_pe_timestamp
from orthia_runner import EXIT_COMMAND_ERROR

pytestmark = [pytest.mark.pe, pytest.mark.usefixtures("self_pe")]


@pytest.fixture(scope="module")
def me(data, self_pe):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def dep(self_pe):
    """(dll, function) as lm and x name them."""
    return host_import(self_pe)


def module_base(res, module):
    return int(res.assert_line(rf"^(\S+)\s+\S+\s+{re.escape(module)}\s").group(1).replace("`", ""), 16)


def addr_text(address):
    return f"{address >> 32:08x}`{address & 0xFFFFFFFF:08x}"


def test_lm_shows_linked_dependencies(orthia, me, self_pe):
    res = orthia.run("lm", **me).assert_ok()
    for dll in self_pe.imports:
        res.assert_line(rf"\b{re.escape(dll.lower())}\s+linked$")


def test_db_dependency_header(orthia, me, dep):
    dll, _ = dep
    res = orthia.run("lm", f"db {dll} L2", **me).assert_ok()
    res.assert_line(rf"^{addr_text(module_base(res, dll))}  4d 5a\s+MZ$")


def test_u_dependency_export(orthia, me, dep):
    dll, function = dep
    export = orthia.run(f"x {dll}!{function}", **me).assert_ok().symbols[f"{dll}!{function}"][0]
    res = orthia.run(f"u {dll}!{function} L2", **me).assert_ok()
    assert res.first_addr() == export, res
    assert len(res.instructions()) == 2, res


def test_modinfo_dependency(orthia, me, dep):
    dll, _ = dep
    res = orthia.run(f"modinfo {dll}", **me).assert_ok()
    res.assert_line(r"^Image: linked")
    guid = res.assert_line(r"^Debug GUID: (\S+)$").group(1)
    assert guid.strip("0-"), res
    res.assert_line(rf"(?i)^Pdb name: {re.escape(dll.rsplit('.', 1)[0])}\.pdb$")


def test_analyze_dependency(orthia_cold, me, dep):
    dll, _ = dep
    res = orthia_cold.run(f".analyze {dll}", "lm", **me).assert_ok()
    res.assert_line(rf"\b{re.escape(dll)}\s+linked, analysis$")


def test_iat_slot_is_linked(orthia, me, dep, data):
    # the import table of the main module holds the dependency's export address, on a warm
    # reopen too: the first open links it, later opens replay the recorded slots
    dll, function = dep
    slot = iat_slot(data.self_file, dll, function)
    export = orthia.run(f"x {dll}!{function}", **me).assert_ok().symbols[f"{dll}!{function}"][0]
    res = orthia.run(f"dps {slot:x} L1", **me).assert_ok()
    line = res.assert_line(rf"^{addr_text(slot)}\s+(\S+)\s+{re.escape(dll)}!{function}$")
    assert int(line.group(1).replace("`", ""), 16) == export, res


@pytest.fixture
def target(data, dep, tmp_path):
    """A copy of the binary under test with its own copy of the dependency beside it."""
    return data.copy_with_dependency(data.self_file, dep[0], tmp_path / "target")


def _dependency_is(res, dll, status):
    res.assert_line(rf"\b{re.escape(dll)}\s+{status}$")


def test_co_located_dependency_wins(orthia_cold, target, dep):
    exe, copy = target
    dll, _ = dep
    res = orthia_cold.run("lm", f"modinfo {dll}", file=exe).assert_ok()
    _dependency_is(res, dll, "linked")
    full_name = res.assert_line(r"^Full name: (.+)$").group(1)
    assert Path(full_name).resolve() == copy.resolve(), res


def test_stale_after_timestamp_change(orthia_cold, target, dep):
    exe, copy = target
    dll, function = dep
    first = orthia_cold.run("lm", file=exe).assert_ok()
    _dependency_is(first, dll, "linked")
    base = module_base(first, dll)

    patch_pe_timestamp(copy)
    res = orthia_cold.run("lm", f"db {dll} L2", f"modinfo {dll}", f"x {dll}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, dll, "stale")
    res.assert_line(rf"^{addr_text(base)}  \?\? \?\?")
    res.assert_line(r"^Image: stale \(timestamp mismatch\)$")
    assert f"No image data for module: {dll}" in res.stdout, res
    # the names recorded at first open survive
    assert f"{dll}!{function}" in res.symbols, res


def test_stale_after_guid_change(orthia_cold, target, dep):
    exe, copy = target
    dll, _ = dep
    first = orthia_cold.run("lm", file=exe).assert_ok()
    _dependency_is(first, dll, "linked")
    base = module_base(first, dll)

    # the headers still match, so the file is only found out when it is mapped
    flip_rsds_guid(copy)
    res = orthia_cold.run("lm", f"db {dll} L2", "lm", file=exe)
    rows = re.findall(rf"\b{re.escape(dll)}\s+(\w+)$", res.stdout, re.M)
    assert rows == ["linked", "stale"], res
    res.assert_line(rf"^{addr_text(base)}  \?\? \?\?")


def test_stale_after_file_removed(orthia_cold, target, dep):
    exe, copy = target
    dll, function = dep
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), dll, "linked")

    copy.unlink()
    res = orthia_cold.run("lm", f"modinfo {dll}", f"x {dll}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, dll, "stale")
    res.assert_line(r"^Image: stale \(file not found\)$")
    assert f"{dll}!{function}" in res.symbols, res


def test_old_database_dependency_is_unlinked(orthia_cold, target, dep):
    # a database written before image sources existed has no src_kind: names only
    exe, _ = target
    dll, function = dep
    _dependency_is(orthia_cold.run("lm", file=exe).assert_ok(), dll, "linked")

    (db,) = list(orthia_cold.home.rglob("data.db"))
    con = sqlite3.connect(db)
    try:
        changed = con.execute("UPDATE tbl_metainfo SET meta_info = replace(meta_info, ' src_kind=\"linked\"', '') "
                              "WHERE meta_type = 1 AND meta_info LIKE '%src_kind=\"linked\"%'").rowcount
        con.commit()
    finally:
        con.close()
    assert changed > 0

    res = orthia_cold.run("lm", f"modinfo {dll}", f"x {dll}!*", file=exe)
    assert res.code == EXIT_COMMAND_ERROR, res
    _dependency_is(res, dll, "unlinked")
    res.assert_line(r"^Image: none \(recorded without identity\)$")
    assert f"{dll}!{function}" in res.symbols, res
