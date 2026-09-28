"""Quick open (--cmd default) vs --analyze, and upgrading a quick DB to a full one.

tbl_metainfo (exports, private symbols, module flags) is ON DELETE CASCADE from
tbl_modules, and foreign keys are enforced on every connection. An upgrade that
deleted or replaced the module row would silently lose the names, so every upgrade
path checks that names survive, in the session that created the DB and in a later one.
"""
import re
import sqlite3
from pathlib import Path

import pytest

pytestmark = pytest.mark.pe

NT_BASE = 0x1_4000_0000
KE_BUG_CHECK = 0x1_4015_DAE0
KI_SYSTEM_CALL64 = 0x1_4016_F600
XREF = r";\s+<--"   # cross-reference annotation, produced by deep analysis only


def db_counts(home: Path):
    """(names of the main module, references) in the only DB under this home."""
    (db,) = list(Path(home).rglob("data.db"))
    con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
    try:
        names = con.execute("SELECT count(*) FROM tbl_metainfo WHERE meta_mod_id = ?", (NT_BASE,)).fetchone()[0]
        refs = con.execute("SELECT count(*) FROM tbl_references").fetchone()[0]
    finally:
        con.close()
    return names, refs


def assert_names_intact(orthia, nt):
    res = orthia.run("lm", "x ntoskrnl!KeBugCheck", **nt).assert_ok()
    res.assert_line(r"\bntoskrnl\.exe\s+analysis")
    assert res.symbols["ntoskrnl.exe!KeBugCheck"][0] == KE_BUG_CHECK
    assert orthia.resolve("KeBugCheck", **nt) == KE_BUG_CHECK


@pytest.fixture
def nt(data):
    return {"file": data.ntoskrnl}


def test_quick_open(orthia_cold, nt):
    res = orthia_cold.run("lm", "x ntoskrnl!KeBugCheck", "u KeBugCheck L6", **nt).assert_ok()
    res.assert_line(r"\bntoskrnl\.exe$")                  # no "analysis"
    assert res.symbols["ntoskrnl.exe!KeBugCheck"] == [KE_BUG_CHECK]
    assert res.instructions(), res
    assert not any(re.search(XREF, line) for line in res.lines), res
    names, refs = db_counts(orthia_cold.home)
    assert names > 0 and refs == 0


def test_analyze_open(orthia_cold, nt):
    res = orthia_cold.with_(analyze=True).run("lm", "u KeBugCheck L6", **nt).assert_ok()
    res.assert_line(r"\bntoskrnl\.exe\s+analysis$")
    res.assert_line(XREF)
    assert db_counts(orthia_cold.home)[1] > 0


def test_upgrade_in_a_later_session(orthia_cold, nt):
    orthia_cold.run("lm", **nt).assert_ok()
    names_before, _ = db_counts(orthia_cold.home)

    full = orthia_cold.with_(analyze=True)
    full.run("u KeBugCheck L6", **nt).assert_ok().assert_line(XREF)
    assert_names_intact(full, nt)
    names_after, refs = db_counts(orthia_cold.home)
    assert names_after >= names_before and refs > 0


def test_analyze_command_in_the_creating_session(orthia_cold, nt):
    """.analyze right after a quick first open: the connection that created the DB."""
    res = orthia_cold.run(".analyze ntoskrnl", "lm", "x ntoskrnl!KeBugCheck", **nt).assert_ok()
    res.assert_line(r"\bntoskrnl\.exe\s+analysis$")
    assert res.symbols["ntoskrnl.exe!KeBugCheck"][0] == KE_BUG_CHECK
    assert_names_intact(orthia_cold, nt)
    assert db_counts(orthia_cold.home)[1] > 0


def test_analyze_twice_adds_no_references(orthia_cold, nt):
    full = orthia_cold.with_(analyze=True)
    full.run("lm", **nt).assert_ok()
    _, refs = db_counts(orthia_cold.home)
    full.run(".analyze ntoskrnl", **nt).assert_ok()   # already analysed: no-op
    assert db_counts(orthia_cold.home)[1] == refs


@pytest.mark.symbols
def test_reload_in_quick_mode_loads_private_symbols(orthia_cold, data, nt):
    with_pdb = orthia_cold.with_(symbol_path=data.nt_symbols)
    res = with_pdb.run(".reload ntoskrnl", "db ntoskrnl!KiSystemCall64 L1", **nt).assert_ok()
    assert res.first_addr() == KI_SYSTEM_CALL64


@pytest.mark.symbols
def test_upgrade_keeps_private_symbols(orthia_cold, data, nt):
    with_pdb = orthia_cold.with_(symbol_path=data.nt_symbols)
    with_pdb.run(".reload ntoskrnl", **nt).assert_ok()

    # later session, no symbol path: the PDB names must come from the upgraded DB
    full = orthia_cold.with_(analyze=True)
    full.run("lm", **nt).assert_ok()
    assert full.resolve("ntoskrnl!KiSystemCall64", **nt) == KI_SYSTEM_CALL64
    assert_names_intact(full, nt)
