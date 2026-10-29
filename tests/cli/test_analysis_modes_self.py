"""Quick open vs --analyze and upgrading a quick DB, on the binary under test: the counterpart of
test_analysis_modes.py (see there why every upgrade path checks that the names survive).
"""
import re
import sqlite3
import struct
from pathlib import Path

import pytest

from images import dbghelp_symbols

pytestmark = [pytest.mark.pe, pytest.mark.usefixtures("self_pe")]

EXPORT = "pdb_load"
XREF = r";\s+<--"   # cross-reference annotation, produced by deep analysis only


@pytest.fixture
def me(data, self_pe):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def export_addr(self_pe):
    return self_pe.image_base + self_pe.exports[EXPORT]


@pytest.fixture(scope="module")
def entry_call(self_pe):
    """(first direct call at the entry point, its target): analysis starts at the entry point, so it must
    put a cross-reference on that target. An export like EXPORT may only be called from code reached
    through a vtable, which analysis doesn't follow, so whether it gets one depends on the build."""
    entry = self_pe.image_base + self_pe.entry_rva
    code = self_pe.bytes_at(self_pe.entry_rva, 16)   # MSVC: sub rsp, 28h; call __security_init_cookie
    call = code.find(b"\xE8")
    assert call >= 0, f"no call rel32 at the entry point: {code.hex(' ')}"
    target = entry + call + 5 + struct.unpack_from("<i", code, call + 1)[0]
    # an E8 byte inside another instruction would give a target outside the code
    assert any(s.flags.endswith("X") and s.address <= target - self_pe.image_base < s.address + s.size
               for s in self_pe.sections), f"not a call rel32 at the entry point: {code.hex(' ')}"
    return entry + call, target


def xref_from(addr: int) -> str:
    """The cross-reference annotation for a reference from the instruction at addr,
    e.g. ';  <-- 00000001`4021cb28 --'."""
    return XREF + rf"\s+{addr >> 32:08x}`{addr & 0xFFFFFFFF:08x}\b"


def db_counts(home: Path, base: int):
    """(names of the main module, references) in the only DB under this home."""
    (db,) = list(Path(home).rglob("data.db"))
    con = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
    try:
        names = con.execute("SELECT count(*) FROM tbl_metainfo WHERE meta_mod_id = ?", (base,)).fetchone()[0]
        refs = con.execute("SELECT count(*) FROM tbl_references").fetchone()[0]
    finally:
        con.close()
    return names, refs


def assert_names_intact(orthia, me, export_addr):
    res = orthia.run("lm", f"x orthia!{EXPORT}", **me).assert_ok()
    res.assert_line(r"\borthia\.exe\s+analyzed")
    assert res.symbols[f"orthia.exe!{EXPORT}"][0] == export_addr
    assert orthia.resolve(EXPORT, **me) == export_addr


def test_quick_open(orthia_cold, me, self_pe, export_addr):
    res = orthia_cold.run("lm", f"x orthia!{EXPORT}", f"u {EXPORT} L6", **me).assert_ok()
    res.assert_line(r"\borthia\.exe$")                  # no "analyzed"
    assert res.symbols[f"orthia.exe!{EXPORT}"] == [export_addr]
    assert res.instructions(), res
    assert not any(re.search(XREF, line) for line in res.lines), res
    names, refs = db_counts(orthia_cold.home, self_pe.image_base)
    assert names > 0 and refs == 0


def test_analyze_open(orthia_cold, me, self_pe, entry_call):
    call, callee = entry_call
    res = orthia_cold.with_(analyze=True).run("lm", f"u {callee:#x} L6", **me).assert_ok()
    res.assert_line(r"\borthia\.exe\s+analyzed$")
    res.assert_line(xref_from(call))
    assert db_counts(orthia_cold.home, self_pe.image_base)[1] > 0


def test_upgrade_in_a_later_session(orthia_cold, me, self_pe, export_addr, entry_call):
    call, callee = entry_call
    orthia_cold.run("lm", **me).assert_ok()
    names_before, _ = db_counts(orthia_cold.home, self_pe.image_base)

    full = orthia_cold.with_(analyze=True)
    full.run(f"u {callee:#x} L6", **me).assert_ok().assert_line(xref_from(call))
    assert_names_intact(full, me, export_addr)
    names_after, refs = db_counts(orthia_cold.home, self_pe.image_base)
    assert names_after >= names_before and refs > 0


def test_analyze_command_in_the_creating_session(orthia_cold, me, self_pe, export_addr):
    """.analyze right after a quick first open: the connection that created the DB."""
    res = orthia_cold.run(".analyze orthia", "lm", f"x orthia!{EXPORT}", **me).assert_ok()
    res.assert_line(r"\borthia\.exe\s+analyzed$")
    assert res.symbols[f"orthia.exe!{EXPORT}"][0] == export_addr
    assert_names_intact(orthia_cold, me, export_addr)
    assert db_counts(orthia_cold.home, self_pe.image_base)[1] > 0


def test_analyze_twice_adds_no_references(orthia_cold, me, self_pe):
    full = orthia_cold.with_(analyze=True)
    full.run("lm", **me).assert_ok()
    _, refs = db_counts(orthia_cold.home, self_pe.image_base)
    full.run(".analyze orthia", **me).assert_ok()   # already analysed: no-op
    assert db_counts(orthia_cold.home, self_pe.image_base)[1] == refs


@pytest.fixture(scope="module")
def wmain(data, self_pe):
    return dbghelp_symbols(data.self_file, data.self_symbols, ["wmain"])["wmain"]


@pytest.mark.symbols
def test_reload_in_quick_mode_loads_private_symbols(orthia_cold, data, me, wmain):
    with_pdb = orthia_cold.with_(symbol_path=data.self_symbols)
    res = with_pdb.run(".reload orthia", "db orthia!wmain L1", **me).assert_ok()
    assert res.first_addr() == wmain


@pytest.mark.symbols
def test_upgrade_keeps_private_symbols(orthia_cold, data, me, wmain, export_addr):
    with_pdb = orthia_cold.with_(symbol_path=data.self_symbols)
    with_pdb.run(".reload orthia", **me).assert_ok()

    # later session, no symbol path: the PDB names must come from the upgraded DB
    full = orthia_cold.with_(analyze=True)
    full.run("lm", **me).assert_ok()
    assert full.resolve("orthia!wmain", **me) == wmain
    assert_names_intact(full, me, export_addr)


def run_with(orthia, me, *options, cmds=("lm",)):
    """--cmd with these options exactly: the runner's own --no-analysis --no-symbols left out."""
    args = ["--file", str(me["file"]), *options]
    for cmd in cmds:
        args += ["--cmd", cmd]
    return orthia.raw(*args)


def test_default_open_analyzes(orthia_cold, me, self_pe):
    """--cmd without --no-analysis and --no-symbols: the full open."""
    res = run_with(orthia_cold, me).assert_ok()
    res.assert_line(r"\borthia\.exe\s+analyzed$")
    assert db_counts(orthia_cold.home, self_pe.image_base)[1] > 0


def test_analyze_option_is_accepted(orthia_cold, me):
    run_with(orthia_cold, me, "--analyze").assert_ok().assert_line(r"\borthia\.exe\s+analyzed$")


def test_cmd_saves_no_choice(orthia_cold, me):
    """The UI asks what to start on a new database and keeps the answer in meta.xml;
    the options of --cmd are for that run only."""
    run_with(orthia_cold, me, "--no-symbols").assert_ok()
    (meta,) = list(Path(orthia_cold.home).rglob("meta.xml"))
    assert "auto_" not in meta.read_text(encoding="utf-8", errors="replace")


@pytest.mark.symbols
def test_no_analysis_loads_symbols(orthia_cold, data, me, self_pe, wmain):
    with_pdb = orthia_cold.with_(symbol_path=data.self_symbols)
    res = run_with(with_pdb, me, "--no-analysis", cmds=("db orthia!wmain L1", "lm")).assert_ok()
    res.assert_line(r"\borthia\.exe\s+symbols$")     # no "analyzed"
    assert res.first_addr() == wmain
    assert db_counts(orthia_cold.home, self_pe.image_base)[1] == 0


@pytest.mark.symbols
def test_no_symbols_analyzes(orthia_cold, data, me):
    with_pdb = orthia_cold.with_(symbol_path=data.self_symbols)
    res = run_with(with_pdb, me, "--no-symbols", cmds=("lm", "x orthia!wmain")).assert_ok()
    res.assert_line(r"\borthia\.exe\s+analyzed$")    # no "symbols"
    assert "orthia.exe!wmain" not in res.symbols, res
