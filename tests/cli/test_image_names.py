"""The names an image gives its own addresses in file mode: exports, $entrypoint and the code
the loader runs before the entry point ($tls_ for PE, $ifunc_/$init* for ELF).

Uses the committed images in data/pe and data/elf, so it runs on every host. Expected values
come from images.py; the base is where Orthia mapped the module (`lm`).
"""
import re

import pytest

from data import REPO_ROOT
from images import ElfInfo, PeInfo

_LM_START = re.compile(r"^([0-9a-f`]{17})\s+[0-9a-f`]{17}\s+(\S+)", re.M)

PE_FILES = ["tls_x86_64.dll", "tls_aarch64.dll"]
ELF_FILES = ["libstartup_x64.so", "libstartup_arm64.so", "libexports_gnu.so", "libexports_sysv.so"]


def module_names(orthia, path):
    """(base, {name: [address, ...]}) of the opened file's own module"""
    module = path.name.rsplit(".", 1)[0]
    res = orthia.run("lm", f"x {module}!*", file=path).assert_ok()
    starts = {m.group(2): int(m.group(1).replace("`", ""), 16) for m in _LM_START.finditer(res.stdout)}
    prefix = f"{path.name}!"
    names = {name[len(prefix):]: addresses for name, addresses in res.symbols.items() if name.startswith(prefix)}
    return starts[path.name], names


def names_at(names, address):
    return sorted(name for name, addresses in names.items() if address in addresses)


@pytest.mark.pe
@pytest.mark.parametrize("name", PE_FILES)
def test_pe_names(orthia, name):
    path = REPO_ROOT / "data/pe" / name
    pe = PeInfo(path)
    base, names = module_names(orthia, path)
    assert base == pe.image_base

    callbacks = pe.tls_callbacks
    assert callbacks, "the test image has a TLS callback"
    for index, rva in enumerate(callbacks):
        assert names[f"$tls_{index:04x}"] == [base + rva], names
    assert names["$entrypoint"] == [base + pe.entry_rva], names
    for export, rva in pe.exports.items():
        assert names[export] == [base + rva], names


@pytest.mark.elf
@pytest.mark.parametrize("name", ELF_FILES)
def test_elf_exports(orthia, name):
    path = REPO_ROOT / "data/elf" / name
    elf = ElfInfo(path)
    bias, names = module_names(orthia, path)
    assert elf.exported_functions
    for export, value in elf.exported_functions.items():
        # one name per address in file mode: an alias (vfunc, vfunc_v2) or, for an IFUNC's
        # resolver, its "$" name may be the one kept
        aliases = {n for n, v in elf.exported_functions.items() if v == value}
        found = names_at(names, bias + value)
        assert aliases & set(found) or any(n.startswith("$") for n in found), (export, hex(bias + value), names)


@pytest.mark.elf
@pytest.mark.parametrize("name", ["libstartup_x64.so", "libstartup_arm64.so"])
def test_elf_ifunc_resolver_is_the_export(orthia, name):
    path = REPO_ROOT / "data/elf" / name
    elf = ElfInfo(path)
    bias, names = module_names(orthia, path)
    assert names["$ifunc_exp_ifunc"] == [bias + elf.exported_functions["exp_ifunc"]], names
    assert "$init_array_0000" in names and "$init" in names, names
