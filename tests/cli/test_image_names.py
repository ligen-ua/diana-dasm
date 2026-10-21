"""The names an image gives its own addresses in file mode: exports, $entrypoint and the code
the loader runs before the entry point ($tls_ for PE, $ifunc_/$init* for ELF).

Uses the committed images in data/pe and data/elf, so it runs on every host. Expected values
come from images.py; the base is where Orthia mapped the module (`lm`).
"""
import re

import pytest

from data import REPO_ROOT
from images import ElfInfo, PeInfo
from orthia_runner import Orthia

_LM_START = re.compile(r"^([0-9a-f`]{17})\s+[0-9a-f`]{17}\s+(\S+)", re.M)

PE_FILES = ["tls_x86_64.dll", "tls_aarch64.dll"]
ELF_FILES = ["libstartup_x64.so", "libstartup_arm64.so", "libexports_gnu.so", "libexports_sysv.so", "nopie_x64"]


def module_names(orthia, path):
    """(base, {name: [address, ...]}) of the opened file's own module"""
    module = path.name.rsplit(".", 1)[0] if "." in path.name else path.name
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
        address = elf.address(value, bias)
        found = names_at(names, address)
        assert aliases & set(found) or any(n.startswith("$") for n in found), (export, hex(address), names)
    if elf.e_entry:
        assert names["$entrypoint"] == [elf.address(elf.e_entry, bias)], names


@pytest.mark.elf
@pytest.mark.parametrize("name", [
    "libstartup_x64.so",
    "nopie_x64",
])
def test_elf_export_bytes(orthia, name):
    # the names are where the code is: the bytes at each exported function are the file's
    path = REPO_ROOT / "data/elf" / name
    elf = ElfInfo(path)
    bias, _ = module_names(orthia, path)
    export, value = sorted(elf.exported_functions.items())[0]
    address = elf.address(value, bias)
    res = orthia.run(f"db {address:x} L4", file=path).assert_ok()
    expected = " ".join(f"{b:02x}" for b in elf.bytes_at(value, 4))
    res.assert_line(rf"^[0-9a-f`]{{17}}  {expected}\s")


@pytest.mark.elf
@pytest.mark.parametrize("name", ["libstartup_x64.so", "libstartup_arm64.so"])
def test_elf_ifunc_resolver_is_the_export(orthia, name):
    path = REPO_ROOT / "data/elf" / name
    elf = ElfInfo(path)
    bias, names = module_names(orthia, path)
    assert names["$ifunc_exp_ifunc"] == [bias + elf.exported_functions["exp_ifunc"]], names
    assert "$init_array_0000" in names and "$init" in names, names



def _addr_text(address):
    return f"{address >> 32:08x}`{address & 0xFFFFFFFF:08x}"


def _slot_values(res):
    """{slot address: (value, label)} of the `dps` lines"""
    return {int(m.group(1).replace("`", ""), 16): (int(m.group(2).replace("`", ""), 16), m.group(3))
            for m in re.finditer(r"^([0-9a-f`]{17})\s+([0-9a-f`]{17})\s*(\S*)$", res.stdout, re.M)}


@pytest.mark.elf
def test_elf_plt_slots_are_linked(orthia_exe, empty_symbols, tmp_path):
    # linked_x64 imports from libexports_gnu.so beside it: each PLT slot holds the address of the
    # export where the library is mapped, on the first open and on a warm reopen (replayed from the DB)
    exe = ElfInfo(REPO_ROOT / "data/elf/linked_x64")
    lib = ElfInfo(REPO_ROOT / "data/elf/libexports_gnu.so")
    assert sorted(exe.plt_slots) == ["exp_func_07", "exp_func_21"], exe.plt_slots

    # where the two are mapped (a first open of its own)
    res = Orthia(orthia_exe, tmp_path / "probe", empty_symbols).run("lm", file=exe.path).assert_ok()
    res.assert_line(r"\blibexports_gnu\.so\s+linked$")
    starts = {m.group(2): int(m.group(1).replace("`", ""), 16) for m in _LM_START.finditer(res.stdout)}
    # both are linked at 0: the module start is the load bias
    bias, lib_bias = starts["linked_x64"], starts["libexports_gnu.so"]

    orthia = Orthia(orthia_exe, tmp_path / "home", empty_symbols)
    dps = [f"dps {_addr_text(bias + slot)} L1" for slot in exe.plt_slots.values()]
    for run in ("first open", "reopen"):
        res = orthia.run(*dps, file=exe.path).assert_ok()
        slots = _slot_values(res)
        for name, slot in exe.plt_slots.items():
            expected = (lib_bias + lib.exported_functions[name], f"libexports_gnu.so!{name}")
            assert slots.get(bias + slot) == expected, (run, name, res)
