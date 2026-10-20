"""Raw code (`--format raw --base --bits`): the CLI side of the UI's "open as shellcode" dialog.

The blobs are written here, so every expected value is known up front. The same bytes decode
differently in 32 and 64 bit (0x48 is a REX prefix in 64-bit code, `dec eax` in 32-bit code),
which shows that --bits reaches the disassembler.
"""
import hashlib
import re

import pytest

from orthia_runner import EXIT_BAD_ARGUMENT, EXIT_COMMAND_ERROR, EXIT_OPEN_FAILED

pytestmark = pytest.mark.raw

BASE = 0x10000

# 00 push rbp | 01 mov rbp, rsp | 04 call +0e | 09 pop rbp | 0a ret | 0b nop x3 | 0e xor rax, rax | 11 ret
CODE = bytes.fromhex("55 4889e5 e805000000 5d c3 909090 4831c0 c3")
CALLEE = 0x0e

_LM_ROW = re.compile(r"^([0-9a-f`]{17})\s+([0-9a-f`]{17})\s+(\S+)(?:\s+(\S+(?:, \S+)*))?[ \t]*$", re.M)


def lm_rows(res):
    """module -> (start, end, status)"""
    return {m.group(3): (int(m.group(1).replace("`", ""), 16), int(m.group(2).replace("`", ""), 16), m.group(4) or "")
            for m in _LM_ROW.finditer(res.stdout)}


def raw_options(base=BASE, bits=64):
    return ("--format", "raw", "--base", hex(base), "--bits", str(bits))


def region_name(base, size):
    """The name the analyzer gives a range without headers (CModuleManager::ReloadRange)."""
    return f"region_{base:x}_{size:x}"


@pytest.fixture(scope="module")
def blob(data):
    return data.text_file("raw/code.bin", CODE)


@pytest.fixture(scope="module")
def module(blob):
    return region_name(BASE, len(CODE))


def test_lm(orthia, blob, module):
    rows = lm_rows(orthia.run("lm", file=blob, options=raw_options()).assert_ok())
    assert rows == {module: (BASE, BASE + len(CODE), "analysis")}, rows


def test_bytes(orthia, blob):
    res = orthia.run(f"db {BASE:x} L{len(CODE):x}", file=blob, options=raw_options()).assert_ok()
    assert res.first_addr() == BASE
    res.assert_line(r"^[0-9a-f`]{17}  55 48 89 e5 e8 05 00 00-00 5d c3 90 90 90 48 31\s")


def test_u_64(orthia, blob):
    res = orthia.run(f"u {BASE:x} L3", file=blob, options=raw_options(bits=64)).assert_ok()
    lines = res.instructions()
    assert re.search(r"\bpush\s+RBP$", lines[0]), res
    assert re.search(r"\bmov\s+RBP, RSP$", lines[1]), res
    assert re.search(rf"\bcall\s+0*{BASE + CALLEE:X}h\b", lines[2]), res


def test_u_32(orthia, data):
    # its own file (another SHA1): the bits are recorded in the DB of the first open
    blob32 = data.text_file("raw/code32.bin", CODE + b"\0")
    res = orthia.run(f"u {BASE:x} L4", file=blob32, options=raw_options(bits=32)).assert_ok()
    lines = res.instructions()
    assert re.search(r"\bpush\s+EBP$", lines[0]), res
    assert re.search(r"\bdec\s+EAX$", lines[1]), res
    assert re.search(r"\bmov\s+EBP, ESP$", lines[2]), res


def test_no_names(orthia, blob, module):
    # no headers: no exports, and the entry point is the base, so no $entrypoint either
    res = orthia.run(f"x {module}!*", "x *", file=blob, options=raw_options()).assert_ok()
    assert res.symbols == {}, res


def test_callee_found_by_analysis(orthia_full, blob):
    res = orthia_full.run(f"u {BASE + CALLEE:x} L1", file=blob, options=raw_options()).assert_ok()
    caller = BASE + 4
    res.assert_line(rf"<-- {caller >> 32:08x}`{caller & 0xffffffff:08x}\b")


def test_modinfo(orthia, blob, module):
    res = orthia.run(f"modinfo {module}", file=blob, options=raw_options())
    res.assert_line(rf"^Module: {module}$")
    res.assert_line(r"^Image: main$")


@pytest.mark.xfail(reason="B18: modinfo reports raw code as an unknown module format")
def test_modinfo_no_error(orthia, blob, module):
    orthia.run(f"modinfo {module}", file=blob, options=raw_options()).assert_ok()


def test_sections(orthia, blob, module):
    # raw code has no section table
    res = orthia.run(f"sections {module}", file=blob, options=raw_options())
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(rf"^Error: No sections found in module: {module}$")


def test_reopen_without_format(orthia, blob, module):
    # base and bits are saved next to the DB
    orthia.run("lm", file=blob, options=raw_options()).assert_ok()
    res = orthia.run("lm", file=blob).assert_ok()
    assert lm_rows(res) == {module: (BASE, BASE + len(CODE), "analysis")}, res
    res = orthia.run(f"u {BASE:x} L2", file=blob).assert_ok()
    assert re.search(r"\bmov\s+RBP, RSP$", res.instructions()[1]), res


def test_reopen_with_parameters_saved_before_formats(orthia, data):
    # parameters.xml from before the format was saved in it: raw code
    blob = data.text_file("raw/old_params.bin", CODE + b"\2")
    orthia.run("lm", file=blob, options=raw_options()).assert_ok()
    params = orthia.home / "db" / hashlib.sha1(blob.read_bytes()).hexdigest() / "parameters.xml"
    text = params.read_text()
    assert 'format="raw"' in text, text
    params.write_text(text.replace('format="raw" ', ""))
    res = orthia.run("lm", file=blob).assert_ok()
    assert lm_rows(res) == {region_name(BASE, len(CODE) + 1): (BASE, BASE + len(CODE) + 1, "analysis")}, res


def test_unknown_file_is_not_saved(orthia, data):
    # no DB folder for a file that didn't open
    blob = data.text_file("raw/never_opened.bin", CODE + b"\3")
    res = orthia.run("lm", file=blob)
    assert res.code == EXIT_OPEN_FAILED, res
    assert not (orthia.home / "db" / hashlib.sha1(blob.read_bytes()).hexdigest()).exists()


@pytest.mark.xfail(reason="B20: reopening raw code at another base keeps the old region in its DB")
def test_reopen_other_base(orthia, data):
    blob = data.text_file("raw/rebased.bin", CODE + b"\1")
    orthia.run("lm", file=blob, options=raw_options(base=BASE)).assert_ok()
    other = 0x20000
    rows = lm_rows(orthia.run("lm", file=blob, options=raw_options(base=other)).assert_ok())
    assert rows == {region_name(other, len(CODE) + 1): (other, other + len(CODE) + 1, "analysis")}, rows


def test_forced_detected_format(orthia, data):
    own = "pe" if data.self_is_pe else "elf"
    orthia.run("lm", file=data.self_file, options=("--format", own)).assert_ok()


def test_forced_wrong_format(orthia, data):
    other, name = ("elf", "ELF") if data.self_is_pe else ("pe", "PE")
    res = orthia.run("lm", file=data.self_file, options=("--format", other))
    assert res.code == EXIT_OPEN_FAILED, res
    assert f"Invalid {name} image" in res.stderr, res


def test_forced_on_executable(orthia, data):
    # a recognized file opened as raw: the header bytes are code at the base
    head = data.self_file.read_bytes()[:2]
    res = orthia.run(f"db {BASE:x} L2", file=data.self_file, options=raw_options()).assert_ok()
    res.assert_line(rf"^[0-9a-f`]{{17}}  {head[0]:02x} {head[1]:02x}\s")


# argument parsing

@pytest.mark.parametrize("options, message", [
    (["--format", "bogus"], r"Unknown format: bogus \(supported: pe, elf, raw\)"),
    (["--format", "pe", "--base", "0x1000"], r"--base doesn't apply to --format pe"),
    (["--format", "elf", "--bits", "64"], r"--bits doesn't apply to --format elf"),
    (["--format", "raw", "--bits", "64"], r"--format raw needs --base"),
    (["--format", "raw", "--base", "0x1000"], r"--format raw needs --bits"),
    (["--base", "0x1000"], r"--base needs --format raw"),
    (["--bits", "64"], r"--bits needs --format raw"),
    (["--format", "raw", "--base", "1000h", "--bits", "64"], r"Invalid value for --base: 1000h"),
    (["--format", "raw", "--base", "0x1000", "--bits", "16"], r"Invalid value for --bits: 16 \(expected 32 or 64\)"),
])
def test_bad_options(orthia, blob, options, message):
    res = orthia.raw("--file", blob, *options, "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res
    assert re.search(message, res.stderr), res


@pytest.mark.parametrize("option", ["--format", "--base", "--bits"])
def test_missing_value(orthia, blob, option):
    # last: anything after the option would be taken as its value, even --cmd
    res = orthia.raw("--cmd", "lm", "--file", blob, option)
    assert res.code == EXIT_BAD_ARGUMENT, res
    assert f"Missing value for {option}" in res.stderr, res


def test_format_needs_cmd(orthia, blob):
    res = orthia.raw("--file", blob, *raw_options())
    assert res.code == EXIT_BAD_ARGUMENT, res
    assert "--format needs --cmd" in res.stderr, res


def test_format_needs_file(orthia):
    res = orthia.raw("--pid", "self", *raw_options(), "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res
    assert "--format applies to --file only" in res.stderr, res
