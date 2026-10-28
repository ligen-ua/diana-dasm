"""`xr`: the references to an address, or into a range, with their kind (code, addr, data).

The raw blob is written here, so every reference is known up front; the other tests check
references between modules: a module and the floating code it leads to, and ntoskrnl and the
dependency it calls.
"""
import re
import sys

import pytest

from orthia_runner import EXIT_COMMAND_ERROR

BASE = 0x10000

# 00 call 40 | 05 mov rax, [rip+f4] (reads 100) | 0c lea rcx, [rip+2d] (takes 40) | 13 ret
# 40 ret | 100 data
CODE = (bytes.fromhex("e83b000000 488b05f4000000 488d0d2d000000 c3").ljust(0x40, b"\xcc")
        + b"\xc3").ljust(0x100, b"\xcc") + bytes(8)


def addr(value: int) -> str:
    """An x64 address as orthia prints it: 00000000`00010000"""
    text = f"{value:016x}"
    return re.escape(text[:8] + "`" + text[8:])


def raw_options():
    return ("--format", "raw", "--base", hex(BASE), "--arch", "x64")


@pytest.fixture(scope="module")
def blob(data):
    return data.text_file("raw/xrefs.bin", CODE)


@pytest.mark.raw
def test_references_to_code(orthia_full, blob):
    res = orthia_full.run(f"xr {BASE + 0x40:x}", file=blob, options=raw_options()).assert_ok()
    res.assert_line(rf"^{addr(BASE)}\s+code\s+\S+")
    res.assert_line(rf"^{addr(BASE + 0x0c)}\s+addr\s+\S+")
    assert len(res.lines) == 2, res


@pytest.mark.raw
def test_references_to_data(orthia_full, blob):
    res = orthia_full.run(f"xr {BASE + 0x100:x}", file=blob, options=raw_options()).assert_ok()
    res.assert_line(rf"^{addr(BASE + 0x05)}\s+data\s+\S+")
    assert len(res.lines) == 1, res


@pytest.mark.raw
def test_addresses_only(orthia_full, blob):
    res = orthia_full.run(f"xr /a {BASE + 0x40:x}", file=blob, options=raw_options()).assert_ok()
    assert res.lines == [addr(BASE).replace("\\", ""), addr(BASE + 0x0c).replace("\\", "")], res


@pytest.mark.raw
def test_range(orthia_full, blob):
    res = orthia_full.run(f"xr {BASE:x} L{len(CODE):x}", file=blob, options=raw_options()).assert_ok()
    # a header per address that has references, its referrers under it
    headers = [i for i, line in enumerate(res.lines) if line.endswith(":")]
    assert len(headers) == 2, res
    assert re.match(rf"^{addr(BASE + 0x40)}\b", res.lines[headers[0]]), res
    assert re.match(rf"^{addr(BASE + 0x100)}\b", res.lines[headers[1]]), res
    assert re.match(rf"^  {addr(BASE + 0x05)}\s+data\b", res.lines[headers[1] + 1]), res


@pytest.mark.raw
def test_nothing_refers(orthia_full, blob):
    res = orthia_full.run(f"xr {BASE + 0x13:x}", file=blob, options=raw_options()).assert_ok()
    assert res.lines == [], res


@pytest.mark.raw
@pytest.mark.parametrize("args", ["", "/a", "/x 10000"])
def test_usage(orthia, blob, args):
    res = orthia.run(f"xr {args}".strip(), file=blob, options=raw_options())
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: Usage: xr \[/a\] <address> \[L<length>\]")


# the floating code a module leads to: test_floating.py::test_xr_lists_the_hook

# ntoskrnl calls bootvid.dll through its import table: a code reference into the dependency

DEP = "bootvid.dll"
DEP_FUNC = "VidBitBlt"


@pytest.mark.pe
@pytest.mark.private
@pytest.mark.slow
@pytest.mark.skipif(sys.platform != "win32", reason="host DLL")
def test_dependency_called_through_imports(orthia_full, data):
    nt = {"file": data.ntoskrnl}
    export = orthia_full.run(f"x {DEP}!{DEP_FUNC}", **nt).assert_ok().symbols[f"{DEP}!{DEP_FUNC}"][0]
    res = orthia_full.run(f"xr {export:x}", **nt).assert_ok()
    res.assert_line(r"^\S+\s+code\s+nt")
