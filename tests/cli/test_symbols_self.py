"""The binary under test (orthia.exe) with its own PDB on the symbol path: the counterpart of test_symbols.py.

Expected addresses of private symbols come from dbghelp, which reads the same PDB with Microsoft's code.
"""
import re

import pytest

from images import dbghelp_symbols
from orthia_runner import check_x_sort_orders

pytestmark = [pytest.mark.pe, pytest.mark.symbols, pytest.mark.usefixtures("self_pe")]

EXPORT = "pdb_load"
# private (non-exported) functions: a C++ entry point and two C functions of diana_core
PRIVATE = ["wmain", "propagateFloat32NaN_ex", "Diana_InitContext"]


@pytest.fixture(scope="module")
def me(data, self_pe):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def expected(data, self_pe):
    return dbghelp_symbols(data.self_file, data.self_symbols, PRIVATE + ["orthia::RunConsoleMode"])


def test_status_shows_symbols(orthia_self_pdb, me):
    orthia_self_pdb.run("lm", **me).assert_ok().assert_line(r"orthia\.exe\s+analysis, symbols")


@pytest.mark.parametrize("name", PRIVATE)
@pytest.mark.parametrize("module", ["orthia", "orthia.exe"])
def test_private_symbol_resolves(orthia_self_pdb, me, expected, module, name):
    assert orthia_self_pdb.resolve(f"{module}!{name}", **me) == expected[name]


@pytest.mark.parametrize("name", PRIVATE)
def test_private_symbol_in_x(orthia_self_pdb, me, expected, name):
    names = orthia_self_pdb.run(f"x orthia!{name}", **me).assert_ok().symbols
    assert names[f"orthia.exe!{name}"] == [expected[name]]


def test_cpp_symbol_in_x(orthia_self_pdb, me, expected):
    # C++ names are listed decorated
    names = orthia_self_pdb.run("x orthia!*RunConsoleMode*", **me).assert_ok().symbols
    (name,) = [n for n in names if re.match(r"orthia\.exe!\?RunConsoleMode@orthia@@", n)]
    assert names[name] == [expected["orthia::RunConsoleMode"]], names


def test_exports_still_resolve(orthia_self_pdb, me, self_pe):
    assert orthia_self_pdb.resolve(f"orthia!{EXPORT}", **me) == self_pe.image_base + self_pe.exports[EXPORT]


def test_u_shows_private_symbol(orthia_self_pdb, me, expected):
    res = orthia_self_pdb.run("u orthia!wmain L4", **me).assert_ok()
    assert res.first_addr() == expected["wmain"]
    assert len(res.instructions()) == 4, res


def test_x_has_no_duplicates(orthia_self_pdb, me):
    names = orthia_self_pdb.run("x orthia!p*", **me).assert_ok().symbols
    duplicates = {name: addrs for name, addrs in names.items() if len(addrs) > 1}
    assert not duplicates


def test_x_sort_orders(orthia_self_pdb, me):
    rows = check_x_sort_orders(orthia_self_pdb, "orthia!p*", **me)
    kinds = [kind for kind, _, _ in rows]
    # exports first, then the private symbols that are not exports
    assert "exp" in kinds and "prv" in kinds, rows
    assert kinds == sorted(kinds, key=["exp", "imp", "prv"].index), rows


def test_private_symbol_needs_the_pdb(orthia, me):
    # quick open, no symbols: the same name is unknown
    res = orthia.run("db orthia!wmain L1", **me)
    res.assert_line(r"^Error: Unknown variable: ")


@pytest.mark.xfail(reason="B10: failed symbol search prints a line per candidate and no summary")
def test_failed_symbol_search_is_summarised(orthia_cold, data):
    # the PDB on the symbol path doesn't match this copy's GUID
    mismatch = data.pdb_mismatch_copy(data.self_file, "orthia_mismatch.exe")
    res = orthia_cold.with_(analyze=True, symbol_path=data.self_symbols).run("lm", file=mismatch).assert_ok()
    assert "mismatched" not in res.stderr, res
    assert re.search(r"(?i)symbols?\b.*not found|no matching symbol", res.stderr), res


def test_mismatched_pdb_is_not_loaded(orthia_cold, data):
    mismatch = data.pdb_mismatch_copy(data.self_file, "orthia_mismatch2.exe")
    res = orthia_cold.with_(analyze=True, symbol_path=data.self_symbols).run("lm", file=mismatch).assert_ok()
    res.assert_line(r"\borthia_mismatch2\.exe\s+analysis$")
    res = orthia_cold.run("db orthia_mismatch2!wmain L1", file=mismatch)
    res.assert_line(r"^Error: Unknown variable: ")
