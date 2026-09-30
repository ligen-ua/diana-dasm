"""Process mode on orthia's own process (--pid self)."""
import re
import sys

import pytest

from orthia_runner import EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED

pytestmark = pytest.mark.process

SELF = {"pid": "self"}

if sys.platform == "win32":
    RUNTIME_MODULE, RUNTIME_EXPORT = "ntdll", "NtClose"
else:
    RUNTIME_MODULE, RUNTIME_EXPORT = "libc", "malloc"


@pytest.fixture(scope="module")
def runtime(orthia):
    """(module file name, export address) of a system library loaded in every process."""
    res = orthia.run("lm", f"x {RUNTIME_MODULE}*!{RUNTIME_EXPORT}", **SELF).assert_ok()
    for name, addrs in res.symbols.items():
        module, export = name.split("!")
        if export == RUNTIME_EXPORT:
            return module, addrs[0]
    pytest.fail(f"{RUNTIME_MODULE}*!{RUNTIME_EXPORT} not listed by x\n{res}")


def test_lm_lists_self(orthia):
    res = orthia.run("lm", **SELF).assert_ok()
    res.assert_line(r"\borthia(\.exe|_disasm_ui)?$")


def test_lm_lists_self_analyze(orthia_full):
    res = orthia_full.run("lm", **SELF).assert_ok()
    res.assert_line(r"\borthia(\.exe|_disasm_ui)?\s+analysis")


def test_export_resolves(orthia, runtime):
    module, address = runtime
    stem = module.split(".")[0]
    for expression in (f"{module}!{RUNTIME_EXPORT}",
                       f"{stem}!{RUNTIME_EXPORT}",
                       f"{module.upper()}!{RUNTIME_EXPORT.lower()}"):
        assert orthia.resolve(expression, **SELF) == address, expression


def test_module_resolves_to_base(orthia, runtime):
    module, _ = runtime
    res = orthia.run("lm", **SELF).assert_ok()
    base = res.assert_line(rf"^(\S+)\s+\S+\s+{re.escape(module)}\b").group(1)
    assert orthia.resolve(module, **SELF) == int(base.replace("`", ""), 16)


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
@pytest.mark.xfail(reason="B16: open failure doesn't say why ('Can't open process: 4')")
def test_open_failure_reports_reason(orthia):
    res = orthia.raw("--pid", "4", "--cmd", "lm")
    assert res.code == EXIT_OPEN_FAILED, res
    assert re.search(r"(?i)access is denied", res.stderr), res
