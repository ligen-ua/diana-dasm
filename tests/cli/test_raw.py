"""Raw code (`--format raw --base --arch`): the CLI side of the UI's "open as shellcode" dialog.

The blobs are written here, so every expected value is known up front. The same bytes decode
differently as x86 and x64 (0x48 is a REX prefix in 64-bit code, `dec eax` in 32-bit code),
which shows that --arch reaches the disassembler. The ARM words come from aarch64-linux-gnu-as
and arm-linux-gnueabihf-as.
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


def raw_options(base=BASE, arch="x64"):
    return ("--format", "raw", "--base", hex(base), "--arch", arch)


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
    res = orthia.run(f"u {BASE:x} L3", file=blob, options=raw_options(arch="x64")).assert_ok()
    lines = res.instructions()
    assert re.search(r"\bpush\s+RBP$", lines[0]), res
    assert re.search(r"\bmov\s+RBP, RSP$", lines[1]), res
    assert re.search(rf"\bcall\s+0*{BASE + CALLEE:X}h\b", lines[2]), res


def test_u_32(orthia, data):
    # its own file (another SHA1): the architecture is recorded in the DB of the first open
    blob32 = data.text_file("raw/code32.bin", CODE + b"\0")
    res = orthia.run(f"u {BASE:x} L4", file=blob32, options=raw_options(arch="x86")).assert_ok()
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


def test_modinfo_no_error(orthia, blob, module):
    orthia.run(f"modinfo {module}", file=blob, options=raw_options()).assert_ok()


def test_sections(orthia, blob, module):
    # raw code has no section table
    res = orthia.run(f"sections {module}", file=blob, options=raw_options())
    assert res.code == EXIT_COMMAND_ERROR, res
    res.assert_line(rf"^Error: No sections found in module: {module}$")


def test_reopen_without_format(orthia, blob, module):
    # base and architecture are saved next to the DB
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
    (["--format", "elf", "--arch", "x64"], r"--arch doesn't apply to --format elf"),
    (["--format", "raw", "--arch", "x64"], r"--format raw needs --base"),
    (["--format", "raw", "--base", "0x1000"], r"--format raw needs --arch"),
    (["--base", "0x1000"], r"--base needs --format raw"),
    (["--arch", "x64"], r"--arch needs --format raw"),
    (["--format", "raw", "--base", "1000h", "--arch", "x64"], r"Invalid value for --base: 1000h"),
    (["--format", "raw", "--base", "0x1000", "--arch", "64"],
     r"Invalid value for --arch: 64 \(expected x86, x64, arm64, arm, thumb\)"),
    (["--format", "raw", "--base", "0x1000", "--arch", "ARM64"], r"Invalid value for --arch: ARM64"),
    # ARM code starts at an aligned address
    (["--format", "raw", "--base", "0x1002", "--arch", "arm64"],
     r"Invalid value for --base: 0x1002 \(arm64 code starts at a multiple of 4\)"),
    (["--format", "raw", "--base", "0x1002", "--arch", "arm"],
     r"Invalid value for --base: 0x1002 \(arm code starts at a multiple of 4\)"),
    (["--format", "raw", "--base", "0x1001", "--arch", "thumb"],
     r"Invalid value for --base: 0x1001 \(thumb code starts at a multiple of 2\)"),
])
def test_bad_options(orthia, blob, options, message):
    res = orthia.raw("--file", blob, *options, "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res
    assert re.search(message, res.stderr), res


@pytest.mark.parametrize("option", ["--format", "--base", "--arch"])
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


def test_bits_is_gone(orthia, blob):
    res = orthia.raw("--file", blob, "--format", "raw", "--base", "0x1000", "--bits", "64", "--cmd", "lm")
    assert res.code == EXIT_BAD_ARGUMENT, res


# ARM

def _addr(address):
    return f"{address >> 32:08x}`{address & 0xffffffff:08x}"


def _words(words, size=4):
    return b"".join(w.to_bytes(size, "little") for w in words)


# AArch64: 00 stp | 04 mov | 08 bl 14 | 0c ldp | 10 ret | 14 mov x0, #0 | 18 ret | 1c zeros
#          20 bl 28 (nothing calls 20) | 24 ret | 28 mov x0, #1 | 2c ret
A64_CODE = _words([0xa9bf7bfd, 0x910003fd, 0x94000003, 0xa8c17bfd, 0xd65f03c0, 0xd2800000, 0xd65f03c0,
                   0x00000000, 0x94000002, 0xd65f03c0, 0xd2800020, 0xd65f03c0])


@pytest.fixture(scope="module")
def a64_blob(data):
    return data.text_file("raw/code_a64.bin", A64_CODE)


def test_u_arm64(orthia, a64_blob):
    res = orthia.run(f"u {BASE:x} L5", file=a64_blob, options=raw_options(arch="arm64")).assert_ok()
    lines = res.instructions()
    assert re.search(r"\bstp\s+x29, x30, \[sp, #-16\]!$", lines[0]), res
    assert re.search(r"\bmov\s+x29, sp$", lines[1]), res
    assert re.search(rf"\bbl\s+0x{BASE + 0x14:x}\b", lines[2]), res
    assert re.search(r"\bret$", lines[4]), res
    # 4 bytes each
    assert [res_addr(line) for line in lines] == [BASE + 4 * i for i in range(5)], res


def res_addr(line):
    return int(line.split()[0].replace("`", ""), 16)


def test_arm64_callee_found_by_analysis(orthia_full, a64_blob):
    res = orthia_full.run(f"u {BASE + 0x14:x} L1", file=a64_blob, options=raw_options(arch="arm64")).assert_ok()
    res.assert_line(rf"<-- {_addr(BASE + 0x08)}\b")


def test_arm64_scan_finds_unreferenced_code(orthia_full, a64_blob):
    # raw ARM code is scanned: nothing calls 0x20, but it is code, and so is what it calls
    res = orthia_full.run(f"u {BASE + 0x28:x} L1", file=a64_blob, options=raw_options(arch="arm64")).assert_ok()
    res.assert_line(rf"<-- {_addr(BASE + 0x20)}\b")


def test_arm64_reopen_keeps_arch(orthia, data):
    blob = data.text_file("raw/code_a64_reopen.bin", A64_CODE + b"\0\0\0\0")
    orthia.run("lm", file=blob, options=raw_options(arch="arm64")).assert_ok()
    params = orthia.home / "db" / hashlib.sha1(blob.read_bytes()).hexdigest() / "parameters.xml"
    assert 'diana_arch="1"' in params.read_text(), params.read_text()
    res = orthia.run(f"u {BASE:x} L2", file=blob).assert_ok()
    assert re.search(r"\bmov\s+x29, sp$", res.instructions()[1]), res


def test_saved_without_arch_reopens_as_x86(orthia, data):
    # parameters.xml from before --arch has no diana_arch: x86
    blob = data.text_file("raw/old_arch.bin", CODE + b"\4")
    orthia.run("lm", file=blob, options=raw_options()).assert_ok()
    params = orthia.home / "db" / hashlib.sha1(blob.read_bytes()).hexdigest() / "parameters.xml"
    text = params.read_text()
    assert 'diana_arch="0"' in text, text
    params.write_text(text.replace(' diana_arch="0"', ""))
    res = orthia.run(f"u {BASE:x} L2", file=blob).assert_ok()
    assert re.search(r"\bmov\s+RBP, RSP$", res.instructions()[1]), res


# AArch32, linked at BASE (arm-linux-gnueabihf-as): the literals hold absolute addresses.
# A32: 00 push | 04 cmp r0, #2 | 08 ldrls pc, [pc, r0, lsl #2] | 0c b 3c | 10..18 .word 1c, 24, 2c
#      1c..2c the cases | 2c ldr r3, [pc, #72]; 30 add r3, pc, r3; 34 blx r3 (PIC, to 80) | 3c..58 addls pc table
#      58 ldr pc, =70 | 5c pop {r4, pc} | 60 f1 (moveq pc, lr) | 70 blx 8c (T32) | 7c, 88 literals
#      8c T32 movs r0, #1; bx lr | 90 unref: push {lr}; bl 60; pop {pc} | 9c zeros
A32_CODE = _words([
    0xe92d4010, 0xe3500002, 0x979ff100, 0xea00000a, 0x0001001c, 0x00010024, 0x0001002c, 0xe3a00001,
    0xea00000d, 0xeb00000d, 0xea00000b, 0xe59f3048, 0xe08f3003, 0xe12fff33, 0xea000007, 0xe3510001,
    0x908ff101, 0xea000004, 0xea000000, 0xea000001, 0xe3a00003, 0xea000000, 0xe59ff028, 0xe8bd8010,
    0xe3500000, 0x01a0f00e, 0xe2800001, 0xe12fff1e, 0xe3a00007, 0xfa000004, 0xe12fff1e, 0x00000048,
    0xe3a00009, 0xe12fff1e, 0x00010070, 0x47702001, 0xe52de004, 0xebfffff1, 0xe49df004, 0x00000000])

# T32: 00 push | 02 bl 22 | 06 cmp r0, #2 | 08 bhi 1e | 0a tbb [pc, r0] | 0e the table | 12, 16, 1a the cases
#      1a ldr r3, =2d; 1c blx r3 | 22 f1: it eq; bxeq lr | 2c f2: movw/movt; 36 blx 3c (A32) | 44 .word 0x1002d
T32_CODE = bytes([
    0x10, 0xb5, 0x00, 0xf0, 0x0e, 0xf8, 0x02, 0x28, 0x09, 0xd8, 0xdf, 0xe8, 0x00, 0xf0, 0x02, 0x04,
    0x06, 0x00, 0x01, 0x20, 0x04, 0xe0, 0x02, 0x20, 0x02, 0xe0, 0x0a, 0x4b, 0x98, 0x47, 0x00, 0x20,
    0x10, 0xbd, 0x00, 0x28, 0x08, 0xbf, 0x70, 0x47, 0x01, 0x30, 0x70, 0x47, 0x40, 0xf2, 0x00, 0x00,
    0xc0, 0xf2, 0x01, 0x00, 0x41, 0x68, 0x00, 0xf0, 0x02, 0xe8, 0x70, 0x47, 0x05, 0x00, 0xa0, 0xe3,
    0x1e, 0xff, 0x2f, 0xe1, 0x2d, 0x00, 0x01, 0x00])


@pytest.fixture(scope="module")
def a32_blob(data):
    return data.text_file("raw/code_a32.bin", A32_CODE)


@pytest.fixture(scope="module")
def t32_blob(data):
    return data.text_file("raw/code_t32.bin", T32_CODE)


def _u(orthia, blob, arch, address, count):
    res = orthia.run(f"u {address:x} L{count}", file=blob, options=raw_options(arch=arch)).assert_ok()
    return res, res.instructions()


def _addr32(address):
    """32-bit code prints 8 digits"""
    return f"{address:08x}"


def test_u_arm(orthia, a32_blob):
    res, lines = _u(orthia, a32_blob, "arm", BASE, 4)
    assert re.search(r"\bpush\s+\{r4, lr\}$", lines[0]), res
    assert re.search(r"\bldrls\s+pc, \[pc, r0, lsl #2\]$", lines[2]), res
    assert [res_addr(line) for line in lines] == [BASE + 4 * i for i in range(4)], res


def test_u_thumb(orthia, t32_blob):
    # 16 and 32-bit instructions
    res, lines = _u(orthia, t32_blob, "thumb", BASE, 3)
    assert re.search(r"\bpush\s+\{r4, lr\}$", lines[0]), res
    assert re.search(rf"\bbl\s+0x{BASE + 0x22:x}\b", lines[1]), res
    assert [res_addr(line) for line in lines] == [BASE, BASE + 2, BASE + 6], res


@pytest.mark.parametrize("case", [0x1c, 0x24, 0x2c])
def test_arm_ldrls_table_targets(orthia_full, a32_blob, case):
    res, _ = _u(orthia_full, a32_blob, "arm", BASE + case, 1)
    res.assert_line(rf"<-- {_addr32(BASE + 0x08)}\b")


def test_arm_display_follows_analysis(orthia_full, a32_blob):
    # the table is data, the code blx goes to is T32
    res, lines = _u(orthia_full, a32_blob, "arm", BASE + 0x10, 3)
    assert [re.search(r"\.word\s+(0x[0-9a-f]+)$", line).group(1) for line in lines] == \
        ["0x0001001c", "0x00010024", "0x0001002c"], res
    res, lines = _u(orthia_full, a32_blob, "arm", BASE + 0x88, 3)
    assert re.search(r"\.word\s+0x00010070$", lines[0]), res
    assert re.search(r"\bmovs\s+r0, #1$", lines[1]) and res_addr(lines[1]) == BASE + 0x8c, res
    assert re.search(r"\bbx\s+lr$", lines[2]) and res_addr(lines[2]) == BASE + 0x8e, res


def test_arm_veneer_pic_and_scan(orthia_full, a32_blob):
    # ldr pc, =target; add r3, pc, r3; blx r3; and f1 called from the function only the scan finds
    res, _ = _u(orthia_full, a32_blob, "arm", BASE + 0x70, 1)
    res.assert_line(rf"<-- {_addr32(BASE + 0x58)}\b")
    res, _ = _u(orthia_full, a32_blob, "arm", BASE + 0x80, 1)
    # add r3, pc, r3 takes the address, blx r3 goes there
    res.assert_line(rf"<-- {_addr32(BASE + 0x30)} \[\.\.\.\]")
    # called from 0x24 and from 0x94, which only the scan finds: more than one, [...]
    res, _ = _u(orthia_full, a32_blob, "arm", BASE + 0x60, 1)
    res.assert_line(rf"<-- {_addr32(BASE + 0x24)} \[\.\.\.\]")


@pytest.mark.parametrize("case", [0x12, 0x16, 0x1a])
def test_thumb_tbb_targets(orthia_full, t32_blob, case):
    res, _ = _u(orthia_full, t32_blob, "thumb", BASE + case, 1)
    res.assert_line(rf"<-- {_addr32(BASE + 0x0a)}\b")


def test_thumb_display_follows_analysis(orthia_full, t32_blob):
    # the tbb table: bytes (and the padding after it)
    res, lines = _u(orthia_full, t32_blob, "thumb", BASE + 0x0e, 4)
    assert [re.search(r"\.byte\s+(0x[0-9a-f]+)$", line).group(1) for line in lines] == \
        ["0x02", "0x04", "0x06", "0x00"], res
    # the A32 function blx goes to, then the literal
    res, lines = _u(orthia_full, t32_blob, "thumb", BASE + 0x3c, 3)
    assert re.search(r"\bmov\s+r0, #5$", lines[0]) and res_addr(lines[1]) == BASE + 0x40, res
    assert re.search(r"\.word\s+0x0001002d$", lines[2]), res
    # ldr r3, =f2 + 1 takes the address, blx r3 goes there: two references
    res, _ = _u(orthia_full, t32_blob, "thumb", BASE + 0x2c, 1)
    res.assert_line(rf"<-- {_addr32(BASE + 0x1a)} \[\.\.\.\]")


def test_thumb_reopen_keeps_arch(orthia, data):
    blob = data.text_file("raw/code_t32_reopen.bin", T32_CODE + b"\0\0")
    orthia.run("lm", file=blob, options=raw_options(arch="thumb")).assert_ok()
    params = orthia.home / "db" / hashlib.sha1(blob.read_bytes()).hexdigest() / "parameters.xml"
    assert 'diana_arch="3"' in params.read_text(), params.read_text()
    res = orthia.run(f"u {BASE:x} L2", file=blob).assert_ok()
    assert re.search(rf"\bbl\s+0x{BASE + 0x22:x}\b", res.instructions()[1]), res
