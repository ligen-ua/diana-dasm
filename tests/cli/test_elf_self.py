"""ELF file mode on the binary under test (the Linux build of orthia): the counterpart of test_elf.py,
and of the ELF cases of test_expressions.py and test_dependencies.py.

Expected values come from images.ElfInfo. Orthia maps a PIE at a load bias of its own choosing,
read from `lm`; every address is that bias plus the vaddr in the file.
"""
import os
import re
import shutil

import pytest

from images import break_section_headers
from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED, check_x_sort_orders

pytestmark = [pytest.mark.elf, pytest.mark.usefixtures("self_elf")]

_LM_ROW = re.compile(r"^([0-9a-f`]{17})\s+([0-9a-f`]{17})\s+(\S+)(?:\s+(\S+(?:, \S+)*))?[ \t]*$", re.M)


def lm_rows(res):
    """module -> (start, end, status)"""
    return {m.group(3): (int(m.group(1).replace("`", ""), 16), int(m.group(2).replace("`", ""), 16), m.group(4) or "")
            for m in _LM_ROW.finditer(res.stdout)}


@pytest.fixture(scope="module")
def me(data, self_elf):
    return {"file": data.self_file}


@pytest.fixture(scope="module")
def bias(orthia, me):
    return lm_rows(orthia.run("lm", **me).assert_ok())["orthia"][0]


def test_lm(orthia, me, self_elf, bias):
    rows = lm_rows(orthia.run("lm", **me).assert_ok())
    assert rows["orthia"] == (bias, bias + self_elf.image_size, ""), rows


def test_lm_header(orthia, me):
    orthia.run("lm", **me).assert_line(r"\bmodule name\s+status\b")


def test_lm_analyze(orthia_full, me):
    orthia_full.run("lm", **me).assert_ok().assert_line(r"\borthia\s+analysis$")


def test_modinfo(orthia, me, self_elf):
    res = orthia.run("modinfo orthia", **me).assert_ok()
    res.assert_line(r"^Module: orthia$")
    res.assert_line(rf"^Build ID: {self_elf.build_id}$")


def test_x_entrypoint(orthia, me, self_elf, bias):
    res = orthia.run("x orthia!$entrypoint", **me).assert_ok()
    assert res.symbols["orthia!$entrypoint"] == [bias + self_elf.e_entry]


def test_u_entrypoint(orthia, me, self_elf, bias):
    res = orthia.run("u orthia!$entrypoint L4", "db orthia!$entrypoint L4", **me).assert_ok()
    assert res.first_addr() == bias + self_elf.e_entry
    assert len(res.instructions()) >= 4, res
    expected = " ".join(f"{b:02x}" for b in self_elf.bytes_at(self_elf.e_entry, 4))
    res.assert_line(rf"^[0-9a-f`]{{17}}  {expected}\s")


def test_x_sort_orders(orthia, me):
    rows = check_x_sort_orders(orthia, "orthia!*", **me)
    assert any(symbol.endswith("!$entrypoint") for _, _, symbol in rows), rows


@pytest.mark.parametrize("expression, offset", [("orthia", 0), ("orthia+10", 0x10), ("orthia-10", -0x10)])
def test_module_expressions(orthia, me, bias, expression, offset):
    assert orthia.resolve(expression, **me) == bias + offset


def test_entrypoint_expression(orthia, me, self_elf, bias):
    assert orthia.resolve("orthia!$entrypoint", **me) == bias + self_elf.e_entry


def test_cold_open_of_fresh_copy(orthia_cold, data):
    fresh = data.fresh_copy(data.self_file, "orthia_fresh")
    orthia_cold.run("lm", file=fresh).assert_ok().assert_line(r"\borthia_fresh\b")


def test_unicode_path_with_spaces(orthia, data):
    path = data.fresh_copy(data.self_file, "ортія", subdir="dir with space/юнікод")
    orthia.run("lm", file=path).assert_ok().assert_line(r"\bортія\b")


@pytest.mark.parametrize("size", [64, 20 * 1024])
def test_truncated_elf_reports_reason(orthia, data, size):
    res = orthia.run("lm", file=data.truncated(data.self_file, size, f"orthia_trunc_{size}"))
    assert res.code == EXIT_OPEN_FAILED, res
    assert "DiException" not in res.stderr, res
    assert re.search(r"(?i)invalid elf image: .*(truncated|malformed)", res.stderr), res


def test_elf_with_bad_section_headers(orthia, data):
    # the program headers are valid (Linux runs it), so it may load, or fail with a clear reason
    path = data.copy(data.self_file, "derived/orthia_bad_shdr")
    break_section_headers(path)
    res = orthia.run("lm", file=path)
    assert "DiException" not in res.stderr, res


def test_reload_reports_result(orthia_cold, me):
    res = orthia_cold.run(".reload", **me).assert_ok()
    assert re.search(r"(?i)symbol.*\borthia\b|\borthia\b.*symbol", res.stdout + res.stderr), res


@pytest.mark.xfail(reason="B2: module name comes from the SHA1-keyed DB, not the opened file")
def test_renamed_copy_uses_its_own_name(orthia, data, me):
    orthia.run("lm", **me).assert_ok()  # make sure the DB for this SHA1 exists
    copy = data.copy(data.self_file, "renamed/orthia_renamed")
    orthia.run("lm", file=copy).assert_ok().assert_line(r"\borthia_renamed\b")


# dependencies (DT_NEEDED)

@pytest.fixture
def alone(data, tmp_path):
    """The binary alone in its folder: no library sits beside it."""
    exe = tmp_path / "alone" / "orthia"
    exe.parent.mkdir()
    shutil.copyfile(data.self_file, exe)
    return {"file": exe}


def test_lm_lists_every_needed_library(orthia_cold, alone, self_elf):
    rows = lm_rows(orthia_cold.run("lm", **alone).assert_ok())
    for lib in self_elf.needed:
        assert lib in rows, (lib, rows)
        assert rows[lib][2].split(",")[0] in ("linked", "unresolved"), (lib, rows)


def test_lm_dependency_ranges_do_not_overlap(orthia_cold, alone):
    rows = lm_rows(orthia_cold.run("lm", **alone).assert_ok())
    unresolved = [(start, end) for start, end, status in rows.values() if status == "unresolved"]
    assert all(0 < start < end for start, end in unresolved), rows
    ranges = sorted((start, end) for start, end, _ in rows.values())
    assert all(prev[1] <= cur[0] for prev, cur in zip(ranges, ranges[1:])), rows


@pytest.fixture
def project_lib(self_elf):
    """A DT_NEEDED library of our own build (found through the build tree's RUNPATH): never on the host's search path."""
    for lib in self_elf.needed:
        if lib.startswith("liborthia_") and self_elf.find_needed(lib):
            return lib
    pytest.skip("the binary under test needs no liborthia_* library")


def test_modinfo_unresolved_dependency(orthia_cold, alone, project_lib):
    res = orthia_cold.run("modinfo " + project_lib, f"db {project_lib} L2", **alone)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(r"^Image: none \(unresolved dependency\)$")
    assert f"No image data for module: {project_lib}" in res.stdout, res
    res.assert_line(r"^[0-9a-f`]{17}  \?\? \?\?")


def test_sections_unresolved_dependency(orthia_cold, alone, project_lib):
    res = orthia_cold.run(f"sections {project_lib}", **alone)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(rf"^Error: No image data for module: {re.escape(project_lib)}$")


def test_unresolved_dependency_survives_reopen(orthia_cold, alone, project_lib):
    first = lm_rows(orthia_cold.run("lm", **alone).assert_ok())
    second = lm_rows(orthia_cold.run("lm", **alone).assert_ok())
    assert second == first, (first, second)
    assert second[project_lib][2] == "unresolved", second


def test_libraries_beside_the_binary_are_linked(orthia_cold, self_elf, tmp_path, data):
    """Every library the build tree's RUNPATH provides, copied next to the binary, gets linked."""
    target = tmp_path / "with_libs"
    target.mkdir()
    shutil.copyfile(data.self_file, target / "orthia")
    copied = []
    for lib in self_elf.needed:
        found = self_elf.find_needed(lib)
        if found:
            shutil.copyfile(found, target / lib)
            copied.append(lib)
    if not copied:
        pytest.skip("no RUNPATH libraries to copy")
    res = orthia_cold.run("lm", *[f"db {lib} L4" for lib in copied], file=target / "orthia").assert_ok()
    rows = lm_rows(res)
    for lib in copied:
        assert rows[lib][2] == "linked", (lib, rows)
    assert res.stdout.count("7f 45 4c 46") == len(copied), res


def test_libc_beside_the_binary_is_linked(orthia_cold, self_elf, tmp_path, data):
    libc = next((p for p in ["/lib/x86_64-linux-gnu/libc.so.6", "/usr/lib64/libc.so.6", "/lib64/libc.so.6"]
                 if os.path.isfile(p)), None)
    if libc is None or "libc.so.6" not in self_elf.needed:
        pytest.skip("no libc.so.6 in a known place")
    target = tmp_path / "with_libc"
    target.mkdir()
    shutil.copyfile(data.self_file, target / "orthia")
    shutil.copyfile(libc, target / "libc.so.6")
    res = orthia_cold.run("lm", "db libc.so.6 L4", file=target / "orthia").assert_ok()
    res.assert_line(r"\blibc\.so\.6\s+linked$")
    res.assert_line(r"  7f 45 4c 46\s+\.ELF$")


# sections: ELF section headers are not loaded, they come from the opened file

def expected_sections(self_elf, bias):
    rows = {}
    for section in self_elf.sections:
        address = None if section.address is None else bias + section.address
        rows.setdefault(section.name, []).append((address, section.size, section.flags))
    return rows


def test_sections(orthia, me, self_elf, bias):
    res = orthia.run("sections orthia", **me).assert_ok()
    res.assert_line(r"^Module: orthia$")
    res.assert_line(r"^Section headers: .*orthia$")
    assert res.sections() == expected_sections(self_elf, bias), res


def test_sections_by_address(orthia, me, self_elf, bias):
    res = orthia.run("sections orthia!$entrypoint + 10", **me).assert_ok()
    res.assert_line(r"^Module: orthia$")
    assert res.sections() == expected_sections(self_elf, bias), res


def test_sections_verbose(orthia, me):
    res = orthia.run("sections -v orthia", **me).assert_ok()
    text = res.stdout.splitlines()
    start = text.index(next(line for line in text if line.startswith(".text ")))
    details = text[start + 1:start + 4]
    assert details == ["    sh_type: SHT_PROGBITS", "    sh_flags: 0000000000000006", "    ALLOC EXECINSTR"], res


@pytest.mark.parametrize("cmd, error", [
    ("sections", r"^Error: Usage: sections \[-v\] <module\|address>$"),
    ("sections nosuchmodule", r"^Error: Module not found: nosuchmodule$"),
    ("sections 1", r"^Error: No module at 00000000`00000001$"),
])
def test_sections_errors(orthia, me, cmd, error):
    res = orthia.run(cmd, **me)
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(error)
