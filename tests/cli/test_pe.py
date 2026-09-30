"""PE file mode on ntoskrnl.exe without a PDB (report section "PE")."""
import re
import sys

import pytest

from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OK, EXIT_OPEN_FAILED

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


def test_u_length_counts_instructions(orthia_full, nt):
    # --analyze: the cross-reference annotation lines only exist after deep analysis
    res = orthia_full.run(f"u {KE_BUG_CHECK:x} L2", **nt).assert_ok()
    assert len(res.instructions()) == 2, res
    # annotations are still shown, just not counted
    res.assert_line(r"; ntoskrnl\.exe!KeBugCheck$")


@pytest.mark.parametrize("length", ["l4", "L 4", "l 4", "l(2*2)", "l0n4", "L1+3"])
def test_u_length_forms(orthia, nt, length):
    expected = orthia.run("u ntoskrnl!KeBugCheck L4", **nt).assert_ok().lines
    res = orthia.run(f"u ntoskrnl!KeBugCheck {length}", **nt).assert_ok()
    assert res.first_addr() == KE_BUG_CHECK
    assert len(res.instructions()) == 4, res
    assert res.lines == expected, res


def test_db_lowercase_length(orthia, nt):
    res = orthia.run(f"db {KE_BUG_CHECK:x} l3", **nt).assert_ok()
    res.assert_line(r"^00000001`4015dae0  48 83 ec\s+H\.\.$")


@pytest.mark.parametrize("length", ["L?4", "L-4", "L4 5"])
def test_u_invalid_length(orthia, nt, length):
    res = orthia.run(f"u ntoskrnl!KeBugCheck {length}", **nt)
    assert res.code == EXIT_COMMAND_ERROR, res


def test_x_without_module_searches_main_module(orthia, nt):
    names = orthia.run("x Ke*", **nt).assert_ok().symbols
    assert "ntoskrnl.exe!KeBugCheck" in names
    assert all(n.startswith("ntoskrnl.exe!") for n in names), names


@pytest.mark.xfail(reason="B5: truncated PE rejected with 'DiException, errorCode = -11'")
def test_truncated_pe_reports_reason(orthia, data):
    res = orthia.run("lm", file=data.truncated(data.ntoskrnl, 1024, "trunc_pe.exe"))
    assert res.code == EXIT_OPEN_FAILED, res
    assert "DiException" not in res.stderr, res


@pytest.fixture(scope="module")
def unresolved(orthia, nt):
    # a fixture, not a call inside the xfail tests: if none is left, that must error, not xfail
    return orthia.unresolved_module(**nt)


@pytest.mark.skipif(sys.platform != "win32", reason="host driver")
def test_lm_resolves_driver_import(orthia, nt):
    # cng.sys lives in System32\drivers, outside the default DLL search order
    m = orthia.run("lm", **nt).assert_ok().assert_line(r"^(\S+)\s+(\S+)\s+cng\.sys\b")
    assert m.group(1) != m.group(2), m.string


def test_lm_unresolved_modules_get_own_ranges(orthia, nt):
    # they used to share address 0, and all but one were overwritten in the module table
    res = orthia.run("lm", **nt).assert_ok()
    rows = [(int(m.group(1).replace("`", ""), 16), int(m.group(2).replace("`", ""), 16), bool(m.group(3)))
            for m in re.finditer(r"^([0-9a-f`]{17})\s+([0-9a-f`]{17})\s+\S+(\s+unresolved\b)?", res.stdout, re.M)]
    unresolved = [(start, end) for start, end, flag in rows if flag]
    assert len(unresolved) > 1, res
    assert all(0 < start < end for start, end in unresolved), res
    ranges = sorted((start, end) for start, end, _ in rows)
    assert all(prev[1] <= cur[0] for prev, cur in zip(ranges, ranges[1:])), res


# "unresolved" is an import no host has (e.g. an ext-ms-win-* API set), hal.dll comes from the Windows host
@pytest.mark.parametrize("module", [
    "unresolved",
    pytest.param("hal.dll", marks=pytest.mark.skipif(sys.platform != "win32", reason="host DLL")),
])
def test_modinfo_dependency_without_image(orthia, nt, request, module):
    if module == "unresolved":
        module = request.getfixturevalue("unresolved")
    res = orthia.run(f"modinfo {module}", **nt)
    if res.code == EXIT_OK:
        guid = res.assert_line(r"^Debug GUID: (\S+)$").group(1)
        assert guid.strip("0-"), res
    else:
        assert res.code == EXIT_COMMAND_ERROR, res


NT_IMAGE_END = 0x1_4081_EBB8


def test_u_unmapped(orthia, nt):
    res = orthia.run("u 0 L2", **nt)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^00000000`00000000\s+\?\?\s+\?\?\?$")
    assert "Memory access error" in res.stdout + res.stderr, res


def test_u_unresolved_module(orthia, nt, unresolved):
    # by address: the expression parser cannot take an API set name, its '-' is a minus
    lm = orthia.run("lm", **nt).assert_ok()
    start = lm.assert_line(rf"^(\S+)\s+\S+\s+{re.escape(unresolved)}\s").group(1)
    res = orthia.run(f"u {start} L2", **nt)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"\?\?\s+\?\?\?$")


def test_u_stops_at_end_of_image(orthia, nt):
    res = orthia.run(f"u {NT_IMAGE_END - 0x18:x} L10", **nt)
    assert res.code == EXIT_COMMAND_ERROR, res
    assert res.first_addr() == NT_IMAGE_END - 0x18, res
    res.assert_line(r"^00000001`4081ebb8\s+\?\?\s+\?\?\?$")


def test_db_across_end_of_image(orthia, nt):
    # a range starting inside the image and running past its end used to be all ??
    res = orthia.run(f"db {NT_IMAGE_END - 0x18:x} L20", **nt).assert_ok()
    res.assert_line(r"^00000001`4081eba0  e0 aa e8 aa")
    res.assert_line(r"^00000001`4081ebb0  40 ab 58 ab 60 ab 68 ab-\?\? \?\?")


@pytest.mark.xfail(reason="B12: .analyze on a module without image data does nothing and exits 0")
def test_analyze_module_without_image(orthia_cold, nt, unresolved):
    res = orthia_cold.run(f".analyze {unresolved}", "lm", **nt)
    if res.code == EXIT_OK:
        res.assert_line(rf"\b{re.escape(unresolved)}\s+.*\banalysis")
    else:
        assert res.code == EXIT_COMMAND_ERROR, res
