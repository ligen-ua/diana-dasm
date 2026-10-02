"""ELF files (report section "ELF")."""
import re
import shutil
import sys

import pytest

from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED, check_x_sort_orders

pytestmark = pytest.mark.elf

DMESG_BUILD_ID = "21342db32e32e961622c0d19322ad591d56a65a8"


@pytest.fixture(scope="module")
def dmesg(data):
    return {"file": data.elf("dmesg")}


def test_lm(orthia, dmesg):
    res = orthia.run("lm", **dmesg).assert_ok()
    res.assert_line(r"^00000000`00004000\s+00000000`00014cc8\s+dmesg$")


def test_lm_analyze(orthia_full, dmesg):
    orthia_full.run("lm", **dmesg).assert_ok().assert_line(r"\bdmesg\s+analysis$")


def test_modinfo(orthia, dmesg):
    res = orthia.run("modinfo dmesg", **dmesg).assert_ok()
    res.assert_line(r"^Module: dmesg$")
    res.assert_line(rf"^Build ID: {DMESG_BUILD_ID}$")


def test_x_entrypoint(orthia, dmesg):
    res = orthia.run("x dmesg!*", **dmesg).assert_ok()
    assert res.symbols["dmesg!$entrypoint"] == [0xB0E0]


@pytest.mark.parametrize("name, mask", [("dmesg", "dmesg!*"), ("apt-mark", "apt-mark!*")])
def test_x_sort_orders(orthia, data, name, mask):
    rows = check_x_sort_orders(orthia, mask, file=data.elf(name))
    assert any(symbol.endswith("!$entrypoint") for _, _, symbol in rows), rows


def test_u_entrypoint(orthia, dmesg):
    res = orthia.run("u dmesg!$entrypoint L4", **dmesg).assert_ok()
    assert "endbr64" in res.instructions()[0], res


@pytest.mark.parametrize("name", ["dmesg", "apt-mark"])
def test_opens(orthia, data, name):
    orthia.run("lm", file=data.elf(name)).assert_ok()


def test_cold_open_of_fresh_copy(orthia_cold, data):
    fresh = data.fresh_copy(data.elf("apt-mark"), "apt_fresh")
    res = orthia_cold.run("lm", **{"file": fresh}).assert_ok()
    res.assert_line(r"\bapt_fresh\b")


def test_unicode_path_with_spaces(orthia, data):
    path = data.fresh_copy(data.elf("dmesg"), "дмесг", subdir="dir with space/юнікод")
    res = orthia.run("lm", file=path).assert_ok()
    res.assert_line(r"\bдмесг\b")


@pytest.mark.parametrize("size", [64, 20 * 1024])
def test_truncated_elf_is_rejected(orthia, data, size):
    res = orthia.run("lm", file=data.truncated(data.elf("dmesg"), size, f"trunc_{size}"))
    assert res.code == EXIT_OPEN_FAILED, res


@pytest.mark.xfail(reason="B2: module name comes from the SHA1-keyed DB, not the opened file")
def test_renamed_copy_uses_its_own_name(orthia, data, dmesg):
    orthia.run("lm", **dmesg).assert_ok()  # make sure the DB for this SHA1 exists
    copy = data.copy(data.elf("dmesg"), "renamed/dmesg_renamed")
    res = orthia.run("lm", file=copy).assert_ok()
    res.assert_line(r"\bdmesg_renamed\b")


# B9: the header was cut when every module name was shorter than "module name" ('module nstatus')
def test_lm_header(orthia, dmesg):
    orthia.run("lm", **dmesg).assert_line(r"\bmodule name\s+status\b")


@pytest.mark.skipif(sys.platform != "win32", reason="\\\\?\\ prefix is Windows-only")
def test_modinfo_full_name_has_no_long_path_prefix(orthia, dmesg):
    line = orthia.run("modinfo dmesg", **dmesg).assert_line(r"^Full name: (.*)$")
    assert not line.group(1).startswith("\\\\?\\")


@pytest.mark.parametrize("name, size", [("trunc_64", 64), ("trunc_20480", 20 * 1024)])
def test_truncated_elf_reports_reason(orthia, data, name, size):
    res = orthia.run("lm", file=data.truncated(data.elf("dmesg"), size, name))
    assert res.code == EXIT_OPEN_FAILED, res
    assert "DiException" not in res.stderr, res
    assert re.search(r"(?i)invalid elf image: .*(truncated|malformed)", res.stderr), res


def test_elf_with_bad_section_headers(orthia, data):
    # the program headers are valid (Linux runs it), so it may load, or fail with a clear reason
    res = orthia.run("lm", file=data.elf("ls.bin"))
    assert "DiException" not in res.stderr, res


def test_reload_reports_result(orthia_cold, dmesg):
    res = orthia_cold.run(".reload", **dmesg).assert_ok()
    assert re.search(r"(?i)symbol.*\bdmesg\b|\bdmesg\b.*symbol", res.stdout + res.stderr), res


# DT_NEEDED of apt-mark: a library that cannot be located still gets a module row, as a PE dependency does
APT_MARK_NEEDED = ["libapt-pkg.so.6.0", "libapt-private.so.0.0", "libc.so.6", "libgcc_s.so.1", "libstdc++.so.6"]
_LM_ROW = re.compile(r"^([0-9a-f`]{17})\s+([0-9a-f`]{17})\s+(\S+)(?:\s+(\S+(?:, \S+)*))?[ \t]*$", re.M)


@pytest.fixture
def apt_mark(data, tmp_path):
    # alone in its folder: no library sits beside the exe
    exe = tmp_path / "alone" / "apt-mark"
    exe.parent.mkdir()
    shutil.copyfile(data.elf("apt-mark"), exe)
    return {"file": exe}


def _lm_rows(res):
    return {m.group(3): (int(m.group(1).replace("`", ""), 16), int(m.group(2).replace("`", ""), 16), m.group(4) or "")
            for m in _LM_ROW.finditer(res.stdout)}


def test_lm_lists_every_needed_library(orthia_cold, apt_mark):
    rows = _lm_rows(orthia_cold.run("lm", **apt_mark).assert_ok())
    for lib in APT_MARK_NEEDED:
        assert lib in rows, (lib, rows)
        assert rows[lib][2].split(",")[0] in ("linked", "unresolved"), (lib, rows)
        if sys.platform == "win32":
            assert rows[lib][2] == "unresolved", (lib, rows)


def test_lm_unresolved_elf_dependencies_get_own_ranges(orthia_cold, apt_mark):
    rows = _lm_rows(orthia_cold.run("lm", **apt_mark).assert_ok())
    unresolved = [(start, end) for start, end, status in rows.values() if status == "unresolved"]
    if sys.platform == "win32":
        assert len(unresolved) == len(APT_MARK_NEEDED), rows
    assert all(0 < start < end for start, end in unresolved), rows
    ranges = sorted((start, end) for start, end, _ in rows.values())
    assert all(prev[1] <= cur[0] for prev, cur in zip(ranges, ranges[1:])), rows


@pytest.mark.skipif(sys.platform != "win32", reason="libapt-pkg may exist on a Linux host")
def test_modinfo_unresolved_elf_dependency(orthia_cold, apt_mark):
    res = orthia_cold.run("modinfo libapt-pkg.so.6.0", "db libapt-pkg.so.6.0 L2", **apt_mark)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Image: none \(unresolved dependency\)$")
    assert "No image data for module: libapt-pkg.so.6.0" in res.stdout, res
    res.assert_line(r"^[0-9a-f`]{17}  \?\? \?\?")


@pytest.mark.skipif(sys.platform != "win32", reason="libapt-pkg may exist on a Linux host")
def test_unresolved_elf_dependency_survives_reopen(orthia_cold, apt_mark):
    first = _lm_rows(orthia_cold.run("lm", **apt_mark).assert_ok())
    second = _lm_rows(orthia_cold.run("lm", **apt_mark).assert_ok())
    assert second == first, (first, second)
    assert second["libapt-pkg.so.6.0"][2] == "unresolved", second


# ELF section headers are not loaded: they come from the opened file (dmesg is mapped at 0x4000)
def test_sections(orthia, dmesg):
    res = orthia.run("sections dmesg", **dmesg).assert_ok()
    res.assert_line(r"^Module: dmesg$")
    res.assert_line(r"^Section headers: .*dmesg$")
    rows = res.sections()
    assert len(rows) == 30, res
    assert rows[".text"] == [(0x8D80, 0x6692, "R-X")], res
    assert rows[".interp"] == [(0x4318, 0x1C, "R--")], res
    assert rows[".bss"] == [(0x14920, 0x3A8, "RW-")], res
    # not loaded: no address
    assert rows[".shstrtab"] == [(None, 0x12F, "---")], res


def test_sections_by_address(orthia, dmesg):
    expected = orthia.run("sections dmesg", **dmesg).assert_ok().sections()
    res = orthia.run("sections dmesg!$entrypoint + 10", **dmesg).assert_ok()
    res.assert_line(r"^Module: dmesg$")
    assert res.sections() == expected, res


def test_sections_verbose(orthia, dmesg):
    res = orthia.run("sections -v dmesg", **dmesg).assert_ok()
    text = res.stdout.splitlines()
    start = text.index(next(line for line in text if line.startswith(".text ")))
    details = text[start + 1:start + 4]
    assert details == ["    sh_type: SHT_PROGBITS", "    sh_flags: 0000000000000006", "    ALLOC EXECINSTR"], res


@pytest.mark.parametrize("cmd, error", [
    ("sections", r"^Error: Usage: sections \[-v\] <module\|address>$"),
    ("sections nosuchmodule", r"^Error: Module not found: nosuchmodule$"),
    ("sections 1", r"^Error: No module at 00000000`00000001$"),
])
def test_sections_errors(orthia, dmesg, cmd, error):
    res = orthia.run(cmd, **dmesg)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(error)


@pytest.mark.skipif(sys.platform != "win32", reason="libapt-pkg may exist on a Linux host")
def test_sections_unresolved_elf_dependency(orthia_cold, apt_mark):
    res = orthia_cold.run("sections libapt-pkg.so.6.0", **apt_mark)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Error: No image data for module: libapt-pkg.so.6.0$")
