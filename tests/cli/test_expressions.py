"""Name resolution in expressions without a PDB (regression tests for B3).

Addresses are checked with `db <expr> L1`, which prints the resolved address
even where `u` cannot read memory (dependency modules in file mode).
"""
import re

import pytest

from orthia_runner import EXIT_COMMAND_ERROR

pytestmark = pytest.mark.pe

KE_BUG_CHECK = 0x1_4015_DAE0
NT_BASE = 0x1_4000_0000


@pytest.fixture(scope="module")
def nt(data):
    return {"file": data.ntoskrnl}


@pytest.fixture(scope="module")
def nt_exports(orthia, nt):
    """Export addresses as listed by `x`, the reference the resolver must agree with."""
    res = orthia.run("x ntoskrnl!KeBugCheck*", **nt).assert_ok()
    return {name: addrs[0] for name, addrs in res.symbols.items()}


def test_x_lists_export(nt_exports):
    assert nt_exports["ntoskrnl.exe!KeBugCheck"] == KE_BUG_CHECK


@pytest.mark.parametrize("expression", [
    "KeBugCheck",                      # bare export
    "ntoskrnl!KeBugCheck",             # module without extension
    "ntoskrnl.exe!KeBugCheck",         # module with extension
    "NTOSKRNL.EXE!kebugcheck",         # case-insensitive
])
def test_export_resolves(orthia, nt, nt_exports, expression):
    assert orthia.resolve(expression, **nt) == nt_exports["ntoskrnl.exe!KeBugCheck"]


def test_export_with_offset(orthia, nt):
    assert orthia.resolve("ntoskrnl!KeBugCheck+10", **nt) == KE_BUG_CHECK + 0x10


def test_u_on_export(orthia, nt):
    res = orthia.run("u ntoskrnl!KeBugCheck L6", **nt).assert_ok()
    assert res.first_addr() == KE_BUG_CHECK
    assert any("sub" in line for line in res.instructions()), res


@pytest.mark.parametrize("expression", ["ntoskrnl", "ntoskrnl.exe", "NTOSKRNL"])
def test_module_name_resolves_to_base(orthia, nt, expression):
    assert orthia.resolve(expression, **nt) == NT_BASE


@pytest.mark.parametrize("expression", [
    "NoSuchName",
    "ntoskrnl!NoSuchName",
    "nosuchmodule!KeBugCheck",
    "ntoskrnl!KiSystemCall64",   # private symbol: must not resolve without the PDB
])
def test_unknown_name(orthia, nt, expression):
    res = orthia.run(f"db {expression} L1", **nt)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Unknown variable: ")


@pytest.fixture(scope="module")
def api_set(orthia, nt):
    """(name, base) of an unresolved API set dependency, whose name contains '-'."""
    name = orthia.unresolved_module(**nt)
    lm = orthia.run("lm", **nt).assert_ok()
    start = lm.assert_line(rf"^(\S+)\s+\S+\s+{re.escape(name)}\s").group(1)
    return name, int(start.replace("`", ""), 16)


@pytest.mark.parametrize("with_extension", [True, False], ids=["dll", "stem"])
@pytest.mark.xfail(reason="B18: a module name containing '-' (ext-ms-win-*) is parsed as a subtraction: "
                          "'Invalid token', or 'Unknown variable: ext' without the extension")
def test_module_name_with_dash_resolves_to_base(orthia, nt, api_set, with_extension):
    name, base = api_set
    expression = name if with_extension else name.rsplit(".", 1)[0]
    assert orthia.resolve(expression, **nt) == base


def test_dependency_export(orthia, nt):
    """Dependencies are resolved from the host system (System32), so skip where they are absent."""
    listed = orthia.run("x kdcom!KdSendPacket", **nt).assert_ok().symbols.get("kdcom.dll!KdSendPacket")
    if not listed:
        pytest.skip("kdcom.dll is not available on this host")
    for expression in ("kdcom!KdSendPacket", "kdcom.dll!KdSendPacket", "KdSendPacket"):
        assert orthia.resolve(expression, **nt) == listed[0], expression


@pytest.mark.elf
@pytest.mark.parametrize("expression, expected", [
    ("dmesg", 0x4000),
    ("dmesg+10", 0x4010),
    ("dmesg!$entrypoint", 0xB0E0),
])
def test_elf_expressions(orthia, data, expression, expected):
    assert orthia.resolve(expression, file=data.elf("dmesg")) == expected
