"""Floating code: memory no module owns that the analysis of a module leads to.

orthia_floating_helper (built next to the orthia binary) hooks a function of its own with the
diana patcher: the function jumps to the patcher's stub, the stub calls a handler in an allocation
of its own. `.analyze` on the helper finds the stub as a floating module, and the stub leads to
the handler. It also points an import slot of its own (IAT, GOT on Linux) at a thunk in another
allocation: an import hook."""
import hashlib
import platform
import re
import subprocess
import sys
from pathlib import Path

import pytest

pytestmark = pytest.mark.process

HELPER_NAME = "orthia_floating_helper"
# "00007ff7`a5d70000  00007ff7`a5d71000   fl_7ff7a5d70000   RWX      1        analyzed"
LMF_ROW = r"^(\S+)\s+(\S+)\s+{name}\s+(\S+)\s+(\d+)\s+(\S+)$"


def floating_name(address: int) -> str:
    return f"fl_{address:x}"


class Helper:
    def __init__(self, proc, values):
        self.proc = proc
        self.pid = str(proc.pid)
        self.target = int(values["target"], 16)
        self.stub = int(values["stub"], 16)
        self.handler = int(values["handler"], 16)
        self.bits = int(values["bits"])
        # 0 when the helper found no import slot to hook
        self.slot = int(values.get("slot", "0"), 16)
        self.thunk = int(values.get("thunk", "0"), 16)
        self.import_name = values.get("import", "")
        self.export_slot = int(values.get("export_slot", "0"), 16)
        self.export_thunk = int(values.get("export_thunk", "0"), 16)

    def address_text(self, address: int) -> str:
        """The way orthia prints an address of this process: 00007ff7`a5d91ec0 or 00401000."""
        if self.bits == 32:
            return f"{address:08x}"
        text = f"{address:016x}"
        return text[:8] + "`" + text[8:]

    def change(self):
        self.proc.stdin.write("change\n")
        self.proc.stdin.flush()
        assert self.proc.stdout.readline().startswith("changed")


@pytest.fixture
def helper(orthia_exe):
    machine = platform.machine().lower()
    if machine not in ("amd64", "x86_64", "x86", "i386", "i686", "arm64", "aarch64",
                       "arm", "armv6l", "armv7l", "armv8l"):
        pytest.skip("the diana patcher has no hooks for this architecture")
    exe = orthia_exe.parent / (HELPER_NAME + (".exe" if sys.platform == "win32" else ""))
    if not exe.is_file():
        pytest.skip(f"{exe.name} is not built next to {orthia_exe}")
    proc = subprocess.Popen([str(exe)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    try:
        line = proc.stdout.readline()
        assert line.startswith("ready"), f"the helper didn't start: {line!r}"
        values = dict(re.findall(r"(\w+)=(\w+)", line))
        # the hook works: the handler ran, the original code too
        assert values["calls"] == "1" and values["result"] == "4", line
        yield Helper(proc, values)
    finally:
        # it exits when its stdin is closed
        proc.stdin.close()
        proc.wait(timeout=30)


def lmf_row(res, address: int):
    """(protect, entries, status) of the floating module at address in the lmf output."""
    match = res.assert_line(LMF_ROW.format(name=re.escape(floating_name(address))))
    return match.group(3), int(match.group(4)), match.group(5)


def assert_found(res, helper, address: int, status: str = "analyzed"):
    """The floating module at address: RWX, analyzed, one entry. A 32-bit process may show more:
    the analysis takes an imm32 that falls into the allocation for an address (push 6A0004h)"""
    protect, entries, actual_status = lmf_row(res, address)
    assert (protect, actual_status) == ("RWX", status), res
    if helper.bits == 64:
        assert entries == 1, res
    else:
        assert entries >= 1, res


def test_analyze_finds_the_hook(orthia, helper):
    res = orthia.run(f".analyze {HELPER_NAME}", "lm", "lmf", pid=helper.pid).assert_ok()
    # the stub, then the handler the stub calls through mov rax/eax, <handler> (ARM64: ldr x16, <literal>)
    assert_found(res, helper, helper.stub)
    assert_found(res, helper, helper.handler)
    # lm lists the OS modules only
    lm = res.stdout.split("protect")[0]
    assert "fl_" not in lm, res


def test_lm_without_analyze_has_no_floating_code(orthia, helper):
    res = orthia.run("lmf", pid=helper.pid).assert_ok()
    assert "fl_" not in res.stdout, res


def test_names_and_disassembly(orthia, helper):
    stub = floating_name(helper.stub)
    res = orthia.run(f".analyze {HELPER_NAME}", f"x {stub}!*", f"u {stub}!$entry_0001 L2",
                     pid=helper.pid).assert_ok()
    assert res.symbols[f"{stub}!$entry_0001"] == [helper.stub], res
    # the hooked function leads here, and the stub starts by saving registers (ARM64: the stack for them)
    res.assert_line(rf"<--\s+{re.escape(helper.address_text(helper.target))}")
    stub_text = re.escape(helper.address_text(helper.stub))
    res.assert_line(rf"(?i)^{stub_text}\s+[0-9a-f]+\s+(push\s+rbp|pushad?|sub\s+sp,\s*sp,\s*#0xb0)\b")


def test_modinfo(orthia, helper):
    stub = floating_name(helper.stub)
    res = orthia.run(f".analyze {HELPER_NAME}", f"modinfo {stub}", pid=helper.pid).assert_ok()
    res.assert_line(rf"^Module: {stub}$")
    res.assert_line(r"^Floating code: RWX, private$")
    res.assert_line(r"^Entries: 1$" if helper.bits == 64 else r"^Entries: \d+$")
    # the referrer: the hooked function of the helper, named after the module or its nearest symbol
    res.assert_line(rf"\$entry_0001\s+from {HELPER_NAME}(\.exe)?[+!]")


def test_lmf_verbose_lists_referrers(orthia, helper):
    stub = floating_name(helper.stub)
    handler = floating_name(helper.handler)
    res = orthia.run(f".analyze {HELPER_NAME}", "lmf /v", pid=helper.pid).assert_ok()
    res.assert_line(r"^Referrers:$")
    # the hooked function of the helper leads to the stub, the stub to the handler
    res.assert_line(rf"^  {HELPER_NAME}(\.exe)?\S*\s+-> {stub}!\$entry_0001\s+RWX private")
    res.assert_line(rf"^  {stub}!\S+\s+-> {handler}!\$entry_0001\s+RWX private")


def test_import_slot_hook(orthia, helper):
    if not helper.slot:
        pytest.skip("the helper found no import slot to hook")
    thunk = floating_name(helper.thunk)
    res = orthia.run(f".analyze {HELPER_NAME}", "lmf", "lmf /v", f"modinfo {thunk}",
                     pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.thunk)
    # the slot of the helper leads to the thunk, named by the import
    name = re.escape(helper.import_name)
    res.assert_line(rf"^  {HELPER_NAME}(\.exe)?\+[0-9a-f]+h\s+-> {thunk}!\$entry_0001\s+RWX private "
                    rf"\[import slot: (\S+!)?{name}\]$")
    res.assert_line(rf"\$entry_0001\s+from .*{HELPER_NAME}(\.exe)?\+[0-9a-f]+h \(import slot: (\S+!)?{name}\)")


def test_export_slot_hook(orthia, helper):
    # Windows: the export table entry of the helper's own export
    if not helper.export_slot:
        pytest.skip("the helper hooks no export here")
    thunk = floating_name(helper.export_thunk)
    res = orthia.run(f".analyze {HELPER_NAME}", "lmf", "lmf /v", pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.export_thunk)
    res.assert_line(rf"^  {HELPER_NAME}(\.exe)?\+[0-9a-f]+h\s+-> {thunk}!\$entry_0001\s+RWX private "
                    rf"\[export: floating_exported_function\]$")


def test_lm_has_no_verbose_option(orthia, helper):
    res = orthia.run("lm /v", pid=helper.pid)
    assert "Argument not supported: /v" in res.stdout + res.stderr, res


def test_threads_stack(orthia, helper):
    """The main thread waits in a read of stdin: its stack goes from the C library into the helper.
    Linux takes the registers of a blocked thread from /proc/<pid>/task/<tid>/syscall"""
    if platform.machine().lower() not in ("amd64", "x86_64") or helper.bits != 64:
        pytest.skip("the stack walk is x86-64 only on Linux and on ARM64 Windows")
    res = orthia.run("threads", pid=helper.pid).assert_ok()
    # the first thread is the main one
    stack = res.stdout.split("Stack:", 1)[1].split("\n\n", 1)[0]
    frames = [line.strip() for line in stack.splitlines() if line.strip()]
    assert len(frames) >= 2, res
    assert any(frame.startswith(HELPER_NAME) for frame in frames[1:]), res
    assert "not available" not in frames[0], res


def test_full_analysis_again(orthia, helper):
    res = orthia.run(f".analyze {HELPER_NAME}", f".analyze /f {HELPER_NAME}", "lmf",
                     pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.stub)
    assert_found(res, helper, helper.handler)


def test_analyze_usage(orthia, helper):
    res = orthia.run(".analyze /x helper", pid=helper.pid)
    assert "Usage: .analyze [/f] <module|mask>" in res.stdout + res.stderr, res


def test_analyze_mask(orthia, helper):
    # a mask queues every module it matches; the floating code follows as with a name
    res = orthia.run(".analyze orthia_floating_h*", "lmf", pid=helper.pid).assert_ok()
    res.assert_line(r"^Modules queued: 1$")
    assert_found(res, helper, helper.stub)
    assert_found(res, helper, helper.handler)


def test_analyze_mask_matches_nothing(orthia, helper):
    res = orthia.run(".analyze no_such_module*", pid=helper.pid)
    assert "No module matches: no_such_module*" in res.stdout + res.stderr, res


def test_reopen_keeps_floating_modules_and_marks_stale(orthia_cold, helper):
    orthia_cold.run(f".analyze {HELPER_NAME}", pid=helper.pid).assert_ok()

    # the database of the first open: the modules come back as they were
    res = orthia_cold.run("lmf", pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.stub)
    assert_found(res, helper, helper.handler)

    # the handler allocation changed: kept, but stale
    helper.change()
    res = orthia_cold.run("lmf", f"modinfo {floating_name(helper.handler)}", pid=helper.pid).assert_ok()
    assert lmf_row(res, helper.stub)[2] == "analyzed", res
    assert lmf_row(res, helper.handler)[2] == "stale", res
    res.assert_line(r"^State: stale")

    # .analyze /f checks it against the memory again: analyzed, and not stale on the next open
    res = orthia_cold.run(f".analyze /f {floating_name(helper.handler)}", "lmf", pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.handler)
    res = orthia_cold.run("lmf", pid=helper.pid).assert_ok()
    assert_found(res, helper, helper.handler)



def test_snapshot_is_evidence(orthia_cold, helper):
    handler = floating_name(helper.handler)
    res = orthia_cold.run(f".analyze {HELPER_NAME}", f"modinfo {handler}", pid=helper.pid).assert_ok()
    match = res.assert_line(r"^Snapshot: (.+) \(([0-9a-f]+) bytes(, [0-9a-f]+ unreadable, zero filled)?\)$")
    path = Path(match.group(1))
    sha256 = res.assert_line(r"^SHA-256: ([0-9a-f]{64})$").group(1)
    assert path.is_file(), res
    data = path.read_bytes()
    assert len(data) == int(match.group(2), 16), res
    assert hashlib.sha256(data).hexdigest() == sha256, res
    assert path.name.startswith(f"{helper.handler:x}_{sha256[:16]}"), path

    # the process exits: the folder with the snapshots stays, and no cleanup removes it
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)
    assert path.is_file()
    res = orthia_cold.raw("--cmd", ".database list").assert_ok()
    res.assert_line(rf"^pid {helper.pid}\s.*\(floating code saved\)$")
    orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    assert path.is_file()

    # .database delete does
    orthia_cold.raw("--cmd", f".database delete {helper.pid}").assert_ok()
    assert not path.exists()


def test_saved_folder_after_exit(orthia_cold, helper):
    """The process exits; its folder opens with --proc-folder: the modules from their files,
    the floating code from its snapshots, the analysis from the database"""
    stub = floating_name(helper.stub)
    orthia_cold.run(f".analyze {HELPER_NAME}", pid=helper.pid).assert_ok()
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)

    res = orthia_cold.raw("--proc-folder", helper.pid, "--cmd", "lm", "--cmd", "lmf",
                          "--cmd", f"u {stub}!$entry_0001 L1",
                          "--cmd", f"modinfo {HELPER_NAME}").assert_ok()
    assert_found(res, helper, helper.stub)
    assert_found(res, helper, helper.handler)
    # the helper is still the file it ran from, and so are the system modules (libc: its file maps up to
    # the end of bss, more than the process reports)
    res.assert_line(rf"^\S+\s+\S+\s+{HELPER_NAME}(\.exe)?\s+linked, analyzed$")
    lm = res.stdout.split("protect")[0]
    assert "stale" not in lm, res
    res.assert_line(r"^Image: linked, loaded$")
    stub_text = re.escape(helper.address_text(helper.stub))
    res.assert_line(rf"(?i)^{stub_text}\s+[0-9a-f]+\s+(push\s+rbp|pushad?|sub\s+sp,\s*sp,\s*#0xb0)\b")

    # by its path too, or its data.db; the folder stays
    folders = list((orthia_cold.home / "proc").iterdir())
    assert len(folders) == 1, folders
    res = orthia_cold.raw("--proc-folder", str(folders[0] / "data.db"), "--cmd", "lmf").assert_ok()
    assert_found(res, helper, helper.stub)
    assert (folders[0] / "data.db").is_file()
    assert (folders[0] / "modules.xml").is_file()


def test_saved_folder_not_found(orthia_cold):
    res = orthia_cold.raw("--proc-folder", "no_such_folder_or_pid", "--cmd", "lmf")
    assert res.code == 3, res
    assert "Can't open the saved process folder" in res.stderr, res


def test_capture_after_exit(orthia_cold, helper):
    """.capture, the process exits, its folder opens: the memory as it was, the hook in the
    hooked function (a file on disk has the original bytes), the threads"""
    stub = floating_name(helper.stub)
    res = orthia_cold.run(f".analyze {HELPER_NAME}", ".capture", pid=helper.pid).assert_ok()
    res.assert_line(r"^Captured: \d+ of \d+ regions, [\d.]+ MB \([\d.]+ MB unreadable\), [1-9]\d* threads$")
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)

    res = orthia_cold.raw("--proc-folder", helper.pid, "--cmd", "lm", "--cmd", "lmf",
                          "--cmd", f"u {helper.target:x} L1", "--cmd", "threads",
                          "--cmd", f"modinfo {HELPER_NAME}").assert_ok()
    res.assert_line(rf"^\S+\s+\S+\s+{HELPER_NAME}(\.exe)?\s+captured, analyzed$")
    res.assert_line(r"^Image: captured \(the process memory\)$")
    assert_found(res, helper, helper.stub)
    # the hooked function jumps to the stub (x86 jmp, ARM b / ldr pc)
    target = re.escape(helper.address_text(helper.target))
    res.assert_line(rf"(?i)^{target}\s+[0-9a-f]+\s+(jmp|b|ldr)\b.*{stub}")
    res.assert_line(r"^THREAD ")

    res = orthia_cold.raw("--cmd", ".database list").assert_ok()
    res.assert_line(rf"^pid {helper.pid}\s.*\(captured\)$")
    orthia_cold.raw("--cmd", ".database cleanup").assert_ok()
    assert (next((orthia_cold.home / "proc").iterdir()) / "capture" / "capture.xml").is_file()


def test_capture_option_saves_analyzed_modules(orthia_cold, helper):
    """--capture: the module analyzed is captured, the others come from their files"""
    orthia_cold.run(f".analyze {HELPER_NAME}", pid=helper.pid, options=("--capture",)).assert_ok()
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)
    res = orthia_cold.raw("--proc-folder", helper.pid, "--cmd", "lm", "--cmd", f"u {helper.target:x} L1").assert_ok()
    res.assert_line(rf"^\S+\s+\S+\s+{HELPER_NAME}(\.exe)?\s+captured, analyzed$")
    assert re.search(r"\s+linked$", res.stdout, re.M), res
    target = re.escape(helper.address_text(helper.target))
    res.assert_line(rf"(?i)^{target}\s+[0-9a-f]+\s+(jmp|b|ldr)\b")


def test_capture_needs_a_running_process(orthia_cold, helper):
    orthia_cold.run(f".analyze {HELPER_NAME}", pid=helper.pid).assert_ok()
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)
    res = orthia_cold.raw("--proc-folder", helper.pid, "--cmd", ".capture")
    assert res.code == 1, res
    assert "Nothing to capture: not a running process" in res.stdout + res.stderr, res


SUMMARY = r"^Floating code: \d+ modules?, \d+ referrers?(, \d+ stale)? \(lmf /v lists them\)$"


def test_analysis_prints_a_floating_summary(orthia, helper):
    res = orthia.run(f".analyze {HELPER_NAME}", pid=helper.pid).assert_ok()
    assert re.search(SUMMARY, res.stderr, re.M), res
    res = orthia.run(f".analyze {HELPER_NAME}", ".capture", pid=helper.pid).assert_ok()
    res.assert_line(SUMMARY)


def test_saved_view_stale_against_capture(orthia_cold, helper):
    """found and analyzed live, changed, then .capture: the saved view says the capture differs,
    the snapshot has the bytes that were analyzed"""
    handler = floating_name(helper.handler)
    orthia_cold.run(f".analyze {HELPER_NAME}", pid=helper.pid).assert_ok()
    helper.change()
    orthia_cold.run(".capture", pid=helper.pid).assert_ok()
    helper.proc.stdin.close()
    helper.proc.wait(timeout=30)

    res = orthia_cold.raw("--proc-folder", helper.pid, "--cmd", "lmf", "--cmd", f"modinfo {handler}").assert_ok()
    assert lmf_row(res, helper.handler)[2] == "stale", res
    assert lmf_row(res, helper.stub)[2] == "analyzed", res
    res.assert_line(r"^Snapshot: .+\.bin \(")
    res.assert_line(r"^Capture: .+\.bin \(")
    res.assert_line(r"^State: stale, the captured memory differs from what was found and analyzed")
