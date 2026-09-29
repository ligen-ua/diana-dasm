"""PE file mode on ntoskrnl.exe without a PDB (report section "PE")."""
import sys

import pytest

pytestmark = pytest.mark.pe

KE_BUG_CHECK = 0x1_4015_DAE0


@pytest.fixture(scope="module")
def nt(data):
    return {"file": data.ntoskrnl}


def test_lm(orthia, nt):
    res = orthia.run("lm", **nt).assert_ok()
    res.assert_line(r"^00000001`40000000\s+00000001`4081ebb8\s+ntoskrnl\.exe$")


def test_lm_analyze(orthia_full, nt):
    orthia_full.run("lm", **nt).assert_ok().assert_line(r"\bntoskrnl\.exe\s+analysis$")


def test_modinfo(orthia, nt):
    res = orthia.run("modinfo ntoskrnl", **nt).assert_ok()
    res.assert_line(r"^Module: ntoskrnl\.exe$")
    res.assert_line(r"^Debug GUID: [0-9A-Fa-f-]{36}$")
    res.assert_line(r"ntkrnlmp\.pdb")
    if sys.platform == "win32":
        res.assert_line(r"^Version: 10\.0\.14393\.9512")


def test_x_wildcard(orthia, nt):
    names = orthia.run("x ntoskrnl!KeBugCheck*", **nt).assert_ok().symbols
    assert {"ntoskrnl.exe!KeBugCheck", "ntoskrnl.exe!KeBugCheckEx"} <= set(names)


def test_x_entrypoint(orthia, nt):
    names = orthia.run("x ntoskrnl!$entrypoint", **nt).assert_ok().symbols
    assert "ntoskrnl.exe!$entrypoint" in names


def test_cold_open_of_fresh_copy(orthia_cold, data):
    fresh = data.fresh_copy(data.ntoskrnl, "nt_fresh.exe")
    res = orthia_cold.run("lm", "u nt_fresh!KeBugCheck L6", file=fresh).assert_ok()
    res.assert_line(r"\bnt_fresh\.exe$")
    assert res.instructions(), res


@pytest.mark.slow
def test_cold_open_of_fresh_copy_analyze(orthia_cold, data):
    fresh = data.fresh_copy(data.ntoskrnl, "nt_fresh_analyze.exe")
    res = orthia_cold.with_(analyze=True).run("lm", file=fresh).assert_ok()
    res.assert_line(r"\bnt_fresh_analyze\.exe\s+analysis$")


@pytest.mark.xfail(reason="B1: L counts annotation lines, not instructions")
def test_u_length_counts_instructions(orthia_full, nt):
    # --analyze: the cross-reference annotation lines only exist after deep analysis
    res = orthia_full.run(f"u {KE_BUG_CHECK:x} L2", **nt).assert_ok()
    assert len(res.instructions()) == 2, res


@pytest.mark.xfail(reason="B11: lower-case length prefix is rejected")
def test_u_lowercase_length(orthia, nt):
    orthia.run("u ntoskrnl!KeBugCheck l4", **nt).assert_ok()


def test_x_without_module_searches_main_module(orthia, nt):
    names = orthia.run("x Ke*", **nt).assert_ok().symbols
    assert "ntoskrnl.exe!KeBugCheck" in names
    assert all(n.startswith("ntoskrnl.exe!") for n in names), names
