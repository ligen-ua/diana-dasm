"""Process mode on orthia's own process (--pid self)."""
import os
import re
import sys

import pytest

from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED, check_x_sort_orders

pytestmark = pytest.mark.process

SELF = {"pid": "self"}

if sys.platform == "win32":
    RUNTIME_MODULE, RUNTIME_EXPORT = "ntdll", "NtClose"
else:
    RUNTIME_MODULE, RUNTIME_EXPORT = "libc", "malloc"


def module_base(res, module):
    """Base address of `module` in the `lm` output of the same run."""
    return int(res.assert_line(rf"^(\S+)\s+\S+\s+{re.escape(module)}\b").group(1).replace("`", ""), 16)


@pytest.fixture(scope="module")
def runtime(orthia):
    """(module file name, export offset from the module base) of a system library loaded in
    every process. Each --pid self run is a new process, and with per-process ASLR (Linux)
    the library gets a new base each time, so only offsets compare across runs."""
    res = orthia.run("lm", f"x {RUNTIME_MODULE}*!{RUNTIME_EXPORT}", **SELF).assert_ok()
    for name, addrs in res.symbols.items():
        module, export = name.split("!")
        if export == RUNTIME_EXPORT:
            return module, addrs[0] - module_base(res, module)
    pytest.fail(f"{RUNTIME_MODULE}*!{RUNTIME_EXPORT} not listed by x\n{res}")


def test_lm_lists_self(orthia):
    res = orthia.run("lm", **SELF).assert_ok()
    res.assert_line(r"\borthia(\.exe|_disasm_ui)?$")


def test_lm_lists_self_analyze(orthia_full):
    res = orthia_full.run("lm", **SELF).assert_ok()
    res.assert_line(r"\borthia(\.exe|_disasm_ui)?\s+analysis")


def test_export_resolves(orthia, runtime):
    module, offset = runtime
    # the module name may drop its last extension (ntdll.dll -> ntdll, libc.so.6 -> libc.so)
    # or, when nothing else matches, everything from the first dot (libc.so.6 -> libc)
    expressions = dict.fromkeys((f"{module}!{RUNTIME_EXPORT}",
                                 f"{module.rsplit('.', 1)[0]}!{RUNTIME_EXPORT}",
                                 f"{module.split('.')[0]}!{RUNTIME_EXPORT}",
                                 f"{module.upper()}!{RUNTIME_EXPORT.lower()}"))
    for expression in expressions:
        # db first: first_addr() takes the first address line
        res = orthia.run(f"db {expression} L1", "lm", **SELF).assert_ok()
        assert res.first_addr() - module_base(res, module) == offset, expression


def test_module_resolves_to_base(orthia, runtime):
    module, _ = runtime
    res = orthia.run(f"db {module} L1", "lm", **SELF).assert_ok()
    assert res.first_addr() == module_base(res, module), res


def test_module_resolves_by_name_before_first_dot(orthia):
    # versioned ELF names (libc.so.6, ld-linux-x86-64.so.2) resolve by the part before the
    # first dot, including names with '-' that the expression tokenizer must keep whole
    res = orthia.run("lm", **SELF).assert_ok()
    names = [m.group(1) for m in (re.match(r"^\S+\s+\S+\s+(\S+)", line) for line in res.lines[1:]) if m]
    stems = [name.split(".")[0] for name in names]
    candidates = [name for name, stem in zip(names, stems)
                  if name.count(".") >= 2 and stems.count(stem) == 1 and re.fullmatch(r"[\w-]+", stem)]
    if not candidates:
        pytest.skip("no module with a multi-dot name")
    for name in candidates:
        stem = name.split(".")[0]
        res = orthia.run(f"db {stem} L1", "lm", **SELF).assert_ok()
        assert res.first_addr() == module_base(res, name), stem


def test_x_sort_orders(orthia, runtime):
    module, _ = runtime
    rows = check_x_sort_orders(orthia, f"{module}!{RUNTIME_EXPORT[:2]}*", **SELF)
    assert any(symbol.endswith(f"!{RUNTIME_EXPORT}") for _, _, symbol in rows), rows


def test_unknown_name(orthia, runtime):
    module, _ = runtime
    res = orthia.run(f"db {module}!NoSuchName L1", **SELF)
    assert res.code == 1, res
    res.assert_line(r"^Error: Unknown variable: ")


def test_x_without_module_skips_other_modules(orthia, runtime):
    # the runtime library is not the main module, so a bare name must not find its export
    res = orthia.run(f"x {RUNTIME_EXPORT}", **SELF).assert_ok()
    assert not res.symbols, res


def test_u_unmapped(orthia):
    # page 0 is never mapped; its unread bytes used to be disassembled as zeroes (add [rax], al)
    res = orthia.run("u 0 L2", **SELF)
    assert res.code == EXIT_COMMAND_ERROR, res
    assert not res.instructions() or all("??" in line for line in res.instructions()), res
    res.assert_line(r"^00000000`00000000\s+\?\?\s+\?\?\?$")


@pytest.mark.skipif(sys.platform != "win32", reason="pid 4 (System) is Windows-only")
def test_open_failure_reports_reason(orthia):
    res = orthia.raw("--pid", "4", "--cmd", "lm")
    assert res.code == EXIT_OPEN_FAILED, res
    assert re.search(r"(?i)access is denied", res.stderr), res


def test_sections_of_runtime_library(orthia, runtime):
    module, offset = runtime
    res = orthia.run(f"sections {module}", "lm", **SELF).assert_ok()
    base, end = (int(a.replace("`", ""), 16) for a in
                 res.assert_line(rf"^(\S+)\s+(\S+)\s+{re.escape(module)}\b").groups())
    rows = res.sections()
    if sys.platform != "win32":
        # ELF section headers are not in process memory: read from the library's file
        res.assert_line(rf"^Section headers: /.*{re.escape(module)}$")
    (text_address, text_size, text_flags), = rows[".text"]
    assert text_flags == "R-X", res
    export = base + offset
    assert base < text_address <= export < text_address + text_size <= end + 1, res


@pytest.mark.skipif(sys.platform != "linux" or os.geteuid() == 0, reason="Linux, run as a normal user")
def test_process_of_another_user_explains_ptrace(orthia):
    # pid 1 belongs to root: without ptrace access its memory can't be read
    res = orthia.raw("--pid", "1", "--cmd", "lm")
    if res.code == 0:
        pytest.skip("pid 1 is readable here (a container that runs it as this user)")
    assert res.code == EXIT_OPEN_FAILED, res
    assert re.search(r"^Can't open process: 1: ", res.stderr, re.M), res
    assert "needs ptrace access" in res.stderr and "sudo" in res.stderr, res
