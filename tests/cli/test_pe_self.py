"""PE file mode on the binary under test (orthia.exe) without a PDB: the counterpart of test_pe.py.

Expected values come from images.PeInfo, which reads the file itself, so they follow every rebuild.
"""
import re
import sys

import pytest

from data import REPO_ROOT
from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED, check_x_sort_orders

pytestmark = [pytest.mark.pe, pytest.mark.usefixtures("self_pe")]

EXPORT = "pdb_load"


@pytest.fixture(scope="module")
def me(data, self_pe):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def export_addr(self_pe):
    return self_pe.image_base + self_pe.exports[EXPORT]


def lm_row(res, module):
    m = res.assert_line(rf"^(\S+)\s+(\S+)\s+{re.escape(module)}(?:\s|$)")
    return int(m.group(1).replace("`", ""), 16), int(m.group(2).replace("`", ""), 16)


def test_lm(orthia, me, self_pe):
    res = orthia.run("lm", **me).assert_ok()
    res.assert_line(r"\borthia\.exe$")
    end = self_pe.image_base + max(s.address + s.size for s in self_pe.sections)
    assert lm_row(res, "orthia.exe") == (self_pe.image_base, end), res


def test_lm_analyze(orthia_full, me):
    orthia_full.run("lm", **me).assert_ok().assert_line(r"\borthia\.exe\s+analysis$")


def test_modinfo(orthia, me, self_pe):
    res = orthia.run("modinfo orthia", **me).assert_ok()
    res.assert_line(r"^Module: orthia\.exe$")
    res.assert_line(r"^Debug GUID: [0-9A-Fa-f-]{36}$")
    assert res.assert_line(r"^Pdb name: (.+)$").group(1) == self_pe.pdb_name, res


@pytest.mark.skipif(sys.platform != "win32", reason="the version resource is read on Windows")
def test_modinfo_version(orthia, me):
    header = (REPO_ROOT / "src/orthia/orthia_disasm_ui/orthia_version.h").read_text()
    version = re.search(r'#define ORTHIA_UI_VERSION\s+(.*)', header).group(1)
    parts = [re.search(rf'#define {name}\s+"(\d+)"', header).group(1)
             for name in re.findall(r"(ORTHIA_UI_VER_\w+_STR)", version)]
    res = orthia.run("modinfo orthia", **me).assert_ok()
    res.assert_line(rf"^Version: {', '.join(parts)}$")


def test_x_lists_every_export(orthia, me, self_pe):
    names = orthia.run("x orthia!*", **me).assert_ok().symbols
    expected = {f"orthia.exe!{name}": [self_pe.image_base + rva] for name, rva in self_pe.exports.items()}
    expected["orthia.exe!$entrypoint"] = [self_pe.image_base + self_pe.entry_rva]
    assert names == expected


def test_sections(orthia, me, self_pe):
    res = orthia.run("sections orthia", **me).assert_ok()
    res.assert_line(r"^Module: orthia\.exe$")
    expected = {}
    for section in self_pe.sections:
        expected.setdefault(section.name, []).append((self_pe.image_base + section.address, section.size, section.flags))
    assert res.sections() == expected, res


def test_cold_open_of_fresh_copy(orthia_cold, data):
    fresh = data.fresh_copy(data.self_file, "orthia_fresh.exe")
    res = orthia_cold.run("lm", f"u orthia_fresh!{EXPORT} L6", file=fresh).assert_ok()
    res.assert_line(r"\borthia_fresh\.exe$")
    assert res.instructions(), res


@pytest.mark.slow
def test_cold_open_of_fresh_copy_analyze(orthia_cold, data):
    fresh = data.fresh_copy(data.self_file, "orthia_fresh_analyze.exe")
    res = orthia_cold.with_(analyze=True).run("lm", file=fresh).assert_ok()
    res.assert_line(r"\borthia_fresh_analyze\.exe\s+analysis$")


def test_u_length_counts_instructions(orthia_full, me, export_addr):
    res = orthia_full.run(f"u {export_addr:x} L2", **me).assert_ok()
    assert len(res.instructions()) == 2, res
    # annotations are still shown, just not counted
    res.assert_line(rf"; orthia\.exe!{EXPORT}$")


@pytest.mark.parametrize("length", ["l4", "L 4", "l 4", "l(2*2)", "l0n4", "L1+3"])
def test_u_length_forms(orthia, me, export_addr, length):
    expected = orthia.run(f"u orthia!{EXPORT} L4", **me).assert_ok().lines
    res = orthia.run(f"u orthia!{EXPORT} {length}", **me).assert_ok()
    assert res.first_addr() == export_addr
    assert len(res.instructions()) == 4, res
    assert res.lines == expected, res


def test_db_matches_file_bytes(orthia, me, self_pe, export_addr):
    res = orthia.run(f"db {export_addr:x} l3", **me).assert_ok()
    expected = " ".join(f"{b:02x}" for b in self_pe.bytes_at(self_pe.exports[EXPORT], 3))
    res.assert_line(rf"^{export_addr >> 32:08x}`{export_addr & 0xFFFFFFFF:08x}  {expected}\s")


@pytest.mark.parametrize("length", ["L?4", "L-4", "L4 5"])
def test_u_invalid_length(orthia, me, length):
    res = orthia.run(f"u orthia!{EXPORT} {length}", **me)
    assert res.code == EXIT_COMMAND_ERROR, res


def test_x_sort_orders(orthia, me):
    rows = check_x_sort_orders(orthia, "orthia!pdb_*", **me)
    assert {kind for kind, _, _ in rows} == {"exp"}, rows


@pytest.mark.parametrize("cmd", ["x /q orthia!pdb_*", "x /a", "x /a orthia!pdb_* extra"])
def test_x_usage_errors(orthia, me, cmd):
    res = orthia.run(cmd, **me)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Usage: x \[/a\|/n\] \[/v\] \[module!\]name$")


def test_x_without_module_searches_main_module(orthia, me):
    names = orthia.run("x pdb_*", **me).assert_ok().symbols
    assert f"orthia.exe!{EXPORT}" in names
    assert all(n.startswith("orthia.exe!") for n in names), names


def test_imports_listed_as_dependencies(orthia, me, self_pe):
    res = orthia.run("lm", **me).assert_ok()
    for dll in self_pe.imports:
        res.assert_line(rf"(?i)\s{re.escape(dll)}\s+(linked|unresolved)$")


def test_truncated_pe_is_rejected(orthia, data):
    res = orthia.run("lm", file=data.truncated(data.self_file, 0x200, "orthia_trunc.exe"))
    assert res.code == EXIT_OPEN_FAILED, res
