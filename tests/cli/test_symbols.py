"""ntoskrnl with its matching PDB (from data/pe/nt.zip) on the symbol path."""
import re
import shutil

import pytest

pytestmark = [pytest.mark.pe, pytest.mark.symbols]

KI_SYSTEM_CALL64 = 0x1_4016_F600
KE_BUG_CHECK = 0x1_4015_DAE0


@pytest.fixture(scope="module")
def nt(data):
    return {"file": data.ntoskrnl}


def test_status_shows_symbols(orthia_pdb, nt):
    orthia_pdb.run("lm", **nt).assert_ok().assert_line(r"ntoskrnl\.exe\s+analysis, symbols")


@pytest.mark.parametrize("expression", ["ntoskrnl!KiSystemCall64", "ntoskrnl.exe!KiSystemCall64"])
def test_private_symbol_resolves(orthia_pdb, nt, expression):
    assert orthia_pdb.resolve(expression, **nt) == KI_SYSTEM_CALL64


def test_private_symbol_in_x(orthia_pdb, nt):
    names = orthia_pdb.run("x ntoskrnl!KiSystemCall64", **nt).assert_ok().symbols
    assert names["ntoskrnl.exe!KiSystemCall64"][0] == KI_SYSTEM_CALL64


@pytest.mark.parametrize("expression", ["KeBugCheck", "ntoskrnl!KeBugCheck"])
def test_exports_still_resolve(orthia_pdb, nt, expression):
    assert orthia_pdb.resolve(expression, **nt) == KE_BUG_CHECK


def test_u_shows_private_symbol(orthia_pdb, nt):
    res = orthia_pdb.run("u ntoskrnl!KiSystemCall64 L4", **nt).assert_ok()
    assert res.first_addr() == KI_SYSTEM_CALL64
    assert any("swapgs" in line for line in res.instructions()), res


@pytest.mark.xfail(reason="B7: x lists exported names twice when a PDB is loaded")
def test_x_has_no_duplicates(orthia_pdb, nt):
    names = orthia_pdb.run("x ntoskrnl!KeBugCheck*", **nt).assert_ok().symbols
    duplicates = {name: addrs for name, addrs in names.items() if len(addrs) > 1}
    assert not duplicates


@pytest.mark.xfail(reason="B10: failed symbol search prints a line per candidate and no summary")
def test_failed_symbol_search_is_summarised(orthia_cold, data):
    # the PDB on the symbol path doesn't match this copy's GUID
    mismatch = data.pdb_mismatch_copy(data.ntoskrnl, "nt_mismatch.exe")
    res = orthia_cold.with_(analyze=True, symbol_path=data.nt_symbols).run("lm", file=mismatch).assert_ok()
    assert "mismatched" not in res.stderr, res
    assert re.search(r"(?i)symbols?\b.*not found|no matching symbol", res.stderr), res


@pytest.mark.xfail(reason="B4: .reload loads a PDB into cng.sys, which has no image and a zero GUID")
def test_reload_skips_module_without_image(orthia_cold, data, tmp_path):
    # cng.sys is never found, so it is registered at 0 with size 0 and a zero debug GUID;
    # any cng.pdb on the symbol path must not be taken for it
    symbols = tmp_path / "symbols"
    symbols.mkdir()
    shutil.copyfile(data.nt_symbols / "ntkrnlmp.pdb", symbols / "cng.pdb")
    res = orthia_cold.with_(symbol_path=symbols).run(".reload", "lm", "x cng!*", file=data.ntoskrnl)
    assert not res.symbols, res
    assert not re.search(r"\bcng\.sys\s+.*symbols", res.stdout), res
