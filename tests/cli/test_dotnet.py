""".NET images: a PE with a CLR header directory is marked ".NET" in `lm` and in the "Opened:" line.

An IL-only .NET image has no native entry point (AddressOfEntryPoint 0 in an x64 one), and the mark
is what tells the user why. The images are the committed native tls_x86_64.dll with the CLR
directory entry set: Orthia only looks at the directory, not at the CLR header behind it.
"""
import re
import sqlite3
import struct

import pytest

from data import REPO_ROOT, _pe_headers

CLR_DIRECTORY = 14
NATIVE = REPO_ROOT / "data" / "pe" / "tls_x86_64.dll"


def with_clr_directory(image: bytes, rva=0x1000, size=0x48, directory_count=None) -> bytes:
    image = bytearray(image)
    _, is64, optional = _pe_headers(image)
    count_offset = optional + (108 if is64 else 92)
    if directory_count is not None:
        struct.pack_into("<I", image, count_offset, directory_count)
    struct.pack_into("<II", image, count_offset + 4 + CLR_DIRECTORY * 8, rva, size)
    return bytes(image)


@pytest.fixture(scope="module")
def managed(data):
    return data.text_file("dotnet/tls_managed.dll", with_clr_directory(NATIVE.read_bytes()))


def status(res, module):
    """The status column of the module's `lm` row."""
    match = res.assert_line(rf"\s{re.escape(module)}(\s+(?P<status>\S.*?))?\s*$")
    return match.group("status") or ""


def opened_line(res):
    """The "Opened:" line: the open log goes to stderr."""
    lines = [line for line in res.stderr.splitlines() if line.startswith("Opened: ")]
    assert len(lines) == 1, res
    return lines[0]


def test_lm_marks_managed(orthia, managed):
    res = orthia.run("lm", file=managed).assert_ok()
    assert status(res, "tls_managed.dll") == ".NET", res


def test_opened_line(orthia, managed):
    res = orthia.run("lm", file=managed).assert_ok()
    assert re.match(r"Opened: PE x64, \.NET, base [0-9a-f`]+, entry ", opened_line(res)), res


def test_native_is_not_marked(orthia):
    res = orthia.run("lm", file=NATIVE).assert_ok()
    assert ".NET" not in status(res, "tls_x86_64.dll"), res
    assert opened_line(res).startswith("Opened: PE x64, base "), res


@pytest.mark.parametrize("rva, size, count", [
    (0, 0x48, None),          # no RVA
    (0x1000, 0, None),        # no size
    (0x1000, 0x48, 14),       # the directory is past NumberOfRvaAndSizes
], ids=["no-rva", "no-size", "few-directories"])
def test_unset_directory_is_not_marked(orthia, data, rva, size, count):
    image = with_clr_directory(NATIVE.read_bytes(), rva, size, count)
    name = f"tls_unset_{rva:x}_{size:x}_{count}.dll"
    res = orthia.run("lm", file=data.text_file(f"dotnet/{name}", image)).assert_ok()
    assert ".NET" not in status(res, name), res


def test_first_before_other_statuses(orthia_full, managed):
    # the column is narrow: .NET comes before "analyzed"
    res = orthia_full.run("lm", file=managed).assert_ok()
    assert status(res, "tls_managed.dll") == ".NET, analyzed", res


def test_old_database_gets_the_mark(orthia_cold, data):
    """A database made before the mark existed gets it when the file is opened again."""
    target = data.text_file("dotnet/tls_reopened.dll", with_clr_directory(NATIVE.read_bytes(), size=0x50))
    orthia_cold.run("lm", file=target).assert_ok()
    (db,) = (orthia_cold.home / "db").glob("*/data.db")

    def main_builtin_flags():
        con = sqlite3.connect(db)
        try:
            rows = [r[0] for r in con.execute("SELECT meta_info FROM tbl_metainfo WHERE meta_type = 1")
                    if "tls_reopened" in r[0]]
        finally:
            con.close()
        assert len(rows) == 1, rows
        return int(re.search(r'builtinflags="([0-9a-f]+)"', rows[0]).group(1), 16)

    assert main_builtin_flags() == 0x11
    con = sqlite3.connect(db)
    try:
        con.execute("UPDATE tbl_metainfo SET meta_info = replace(meta_info, 'builtinflags=\"11\"', 'builtinflags=\"1\"') "
                    "WHERE meta_type = 1")
        con.commit()
    finally:
        con.close()
    assert main_builtin_flags() == 0x1

    res = orthia_cold.run("lm", file=target).assert_ok()
    assert status(res, "tls_reopened.dll") == ".NET", res
    assert main_builtin_flags() == 0x11
