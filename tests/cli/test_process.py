"""Process mode on orthia's own process (--pid self)."""
import re
import sys

import pytest

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
