"""PE file mode on a compiled ARM64 image: QcTrEE850.sys, a Qualcomm KMDF driver (data/private/pe).

data/pe/tls_aarch64.dll covers the decoding of a minimal ARM64 PE; this driver adds real compiler output,
kernel imports, extra code sections (TREE_ASM, TREE_CAC, PAGE, INIT) and deep analysis of A64 code.
Its imports are host kernel modules, which are x64 on an x64 host: those must not be linked.
"""
import platform
import re
import sys

import pytest

from data import iat_slot

pytestmark = [pytest.mark.pe, pytest.mark.private]

ENTRY = 0x1_4002_27E0
DEPENDENCIES = ["ntoskrnl.exe", "HAL.dll", "WDFLDR.SYS"]

# the host's kernel modules are of another CPU, or absent (Linux)
not_arm64_host = pytest.mark.skipif(
    sys.platform == "win32" and platform.machine().lower() in ("arm64", "aarch64"),
    reason="an ARM64 Windows host has ARM64 kernel modules to link")


@pytest.fixture(scope="module")
def drv(data):
    return {"file": data.arm64_driver}


def test_lm(orthia, drv):
    res = orthia.run("lm", **drv).assert_ok()
    res.assert_line(r"^00000001`40000000\s+00000001`4002d198\s+QcTrEE850\.sys$")


def test_lm_analyze(orthia_full, drv):
    orthia_full.run("lm", **drv).assert_ok().assert_line(r"\bQcTrEE850\.sys\s+analyzed$")


def test_modinfo(orthia, drv):
    res = orthia.run("modinfo QcTrEE850", **drv).assert_ok()
    res.assert_line(r"^Debug GUID: b957ab5c-5bca-4ad0-8f58-b62e410c9fa8$")
    res.assert_line(r"^Pdb name: .*\\ARM64\\Release\\QcTrEE850\.pdb$")
    if sys.platform == "win32":
        res.assert_line(r"^Version: 1\.0\.9220\.0")


def test_sections(orthia, drv):
    rows = orthia.run("sections QcTrEE850", **drv).assert_ok().sections()
    assert rows["TREE_ASM"] == [(0x1_4000_1000, 0xD8, "R-X")], rows
    assert rows["TREE_CAC"] == [(0x1_4000_2000, 0xC0, "R-X")], rows
    assert rows[".text"] == [(0x1_4000_3000, 0x21066, "R-X")], rows
    assert rows["INIT"] == [(0x1_4002_B000, 0x862, "R-X")], rows


def test_u_entrypoint_is_a64(orthia, drv):
    res = orthia.run("u QcTrEE850!$entrypoint L6", **drv).assert_ok()
    assert res.first_addr() == ENTRY, res
    expected = [r"\bstp\s+x19, x20, \[sp, #-16\]!$", r"\bstp\s+x29, x30, \[sp, #-16\]!$", r"\bmov\s+x29, sp$",
                r"\bmov\s+x20, x0$", r"\bmov\s+x19, x1$", r"\bbl\s+0x14002b1b8\b"]
    lines = res.instructions()
    assert len(lines) == len(expected), res
    for line, pattern in zip(lines, expected):
        assert re.search(pattern, line), (pattern, res)


def test_analysis_reaches_extra_code_sections(orthia_full, drv):
    # TREE_ASM and TREE_CAC are reached only by calls from .text
    res = orthia_full.run("u 140001000 L1", "u 140002000 L1", **drv).assert_ok()
    res.assert_line(r"^00000001`40001000\s+;\s+<-- 00000001`400141bc\b")
    res.assert_line(r"^00000001`40002000\s+;\s+<-- 00000001`4000f1a0\b")


def test_smc_call(orthia_full, drv):
    res = orthia_full.run("u 140001014 L1", **drv).assert_ok()
    res.assert_line(r"^00000001`40001014\s+030000d4\s+smc\s+#0x0$")


def test_init_code_called_from_entrypoint(orthia_full, drv):
    res = orthia_full.run("u 14002b1b8 L1", **drv).assert_ok()
    res.assert_line(r"^00000001`4002b1b8\s+;\s+<-- 00000001`400227f4\b")


@not_arm64_host
def test_host_kernel_modules_are_not_linked(orthia, drv):
    res = orthia.run("lm", **drv).assert_ok()
    for module in DEPENDENCIES:
        res.assert_line(rf"\s{re.escape(module)}\s+unresolved$")


@not_arm64_host
@pytest.mark.parametrize("dll, func", [("WDFLDR.SYS", "WdfVersionBind"), ("ntoskrnl.exe", "ExAllocatePoolWithTag")])
def test_iat_slot_stays_empty(orthia, drv, data, dll, func):
    # it used to hold the address of the host's x64 export, then decoded as A64
    slot = iat_slot(data.arm64_driver, dll, func)
    res = orthia.run(f"dq {slot:x} L1", **drv).assert_ok()
    res.assert_line(rf"^00000001`{slot & 0xFFFFFFFF:08x}\s+00000000`00000000$")
