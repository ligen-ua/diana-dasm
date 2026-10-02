"""Name resolution in expressions without a PDB, on the binary under test: the counterpart of test_expressions.py.

Addresses are checked with `db <expr> L1`, which prints the resolved address
even where `u` cannot read memory. ELF expressions are in test_elf_self.py.
"""
import pytest

from data import host_import
from orthia_runner import EXIT_COMMAND_ERROR

pytestmark = [pytest.mark.pe, pytest.mark.usefixtures("self_pe")]

EXPORT = "pdb_load"


@pytest.fixture(scope="module")
def me(data, self_pe):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def export_addr(self_pe):
    return self_pe.image_base + self_pe.exports[EXPORT]


@pytest.mark.parametrize("expression", [
    EXPORT,                            # bare export
    f"orthia!{EXPORT}",                # module without extension
    f"orthia.exe!{EXPORT}",            # module with extension
    f"ORTHIA.EXE!{EXPORT.upper()}",    # case-insensitive
])
def test_export_resolves(orthia, me, export_addr, expression):
    assert orthia.resolve(expression, **me) == export_addr


def test_export_with_offset(orthia, me, export_addr):
    assert orthia.resolve(f"orthia!{EXPORT}+10", **me) == export_addr + 0x10


def test_dash_is_still_subtraction(orthia, me, export_addr):
    assert orthia.resolve(f"orthia!{EXPORT}-10", **me) == export_addr - 0x10


@pytest.mark.parametrize("expression", ["orthia", "orthia.exe", "ORTHIA"])
def test_module_name_resolves_to_base(orthia, me, self_pe, expression):
    assert orthia.resolve(expression, **me) == self_pe.image_base


@pytest.mark.parametrize("expression", [
    "NoSuchName",
    "orthia!NoSuchName",
    f"nosuchmodule!{EXPORT}",
    "orthia!wmain",   # private symbol: must not resolve without the PDB
])
def test_unknown_name(orthia, me, expression):
    res = orthia.run(f"db {expression} L1", **me)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Unknown variable: ")


@pytest.fixture(scope="module")
def dashed(data, self_pe):
    """A copy whose module name contains '-', the shape of the api-ms-*/ext-ms-* names.
    A fresh copy (own SHA1): an identical one would reuse the DB and its module name (B2)."""
    return {"file": data.fresh_copy(data.self_file, "orthia-self.exe", subdir="dashed")}


@pytest.mark.parametrize("expression", ["orthia-self.exe", "orthia-self"], ids=["exe", "stem"])
def test_module_name_with_dash_resolves_to_base(orthia, dashed, self_pe, expression):
    """B18: a module name containing '-' was parsed as a subtraction."""
    assert orthia.resolve(expression, **dashed) == self_pe.image_base


@pytest.mark.parametrize("module", ["orthia-self.exe", "orthia-self"], ids=["exe", "stem"])
def test_module_name_with_dash_and_offset(orthia, dashed, self_pe, module):
    assert orthia.resolve(f"{module}+10", **dashed) == self_pe.image_base + 0x10
    assert orthia.resolve(f"{module}-10", **dashed) == self_pe.image_base - 0x10
    assert orthia.resolve(f"{module}!{EXPORT}-10", **dashed) == self_pe.image_base + self_pe.exports[EXPORT] - 0x10


def test_dependency_export(orthia, me, self_pe):
    """Dependencies are linked from the host system (System32)."""
    dll, function = host_import(self_pe)
    stem = dll.rsplit(".", 1)[0]
    listed = orthia.run(f"x {stem}!{function}", **me).assert_ok().symbols[f"{dll}!{function}"]
    for expression in (f"{stem}!{function}", f"{dll}!{function}", function):
        assert orthia.resolve(expression, **me) == listed[0], expression
