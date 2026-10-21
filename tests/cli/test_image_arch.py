"""Code in an image decodes as the image's own architecture: ARM64 PE and ELF files are disassembled
as A64, ARM32 ones as T32 or A32, the x64 builds of the same sources as x86.

Uses the committed images in data/pe and data/elf, so it runs on every host.
"""
import re

import pytest

from data import REPO_ROOT

CASES = [
    # file, the code to show, what the first instructions are
    ("pe/tls_aarch64.dll", "tls_aarch64!$entrypoint", [r"\bmov\s+w0, #0x1$", r"\bret$"]),
    ("pe/tls_x86_64.dll", "tls_x86_64!$entrypoint", [r"\bmov\s+EAX, 1$", r"\bret$"]),
    # ARMNT: always T32
    ("pe/tls_armnt.dll", "tls_armnt!$entrypoint", [r"\bmovs\s+r0, #1$", r"\bbx\s+lr$"]),
    ("elf/libstartup_arm64.so", "libstartup_arm64!$ifunc_exp_ifunc",
     [r"\badrp\s+x0, 0x[0-9a-f]+$", r"\badd\s+x0, x0, #0x[0-9a-f]+$", r"\bret$"]),
    ("elf/libstartup_x64.so", "libstartup_x64!$ifunc_exp_ifunc",
     [r"\bendbr64$", r"\blea\s+RAX, ", r"\bret$"]),
    # armhf: T32 by default (the exported functions have the Thumb bit), and -marm
    ("elf/libstartup_thumb.so", "libstartup_thumb!call_local_ifunc",
     [r"\bpush\s+\{r3, lr\}$", r"\bblx\s+0x[0-9a-f]+\b", r"\bpop\s+\{r3, pc\}$"]),
    ("elf/libstartup_arm32.so", "libstartup_arm32!call_local_ifunc",
     [r"\bpush\s+\{r4, lr\}$", r"\bbl\s+0x[0-9a-f]+\b", r"\bpop\s+\{r4, pc\}$"]),
]


@pytest.mark.parametrize("name, where, expected", CASES, ids=[c[0] for c in CASES])
def test_u_decodes_as_image_arch(orthia, name, where, expected):
    res = orthia.run(f"u {where} L{len(expected)}", file=REPO_ROOT / "data" / name).assert_ok()
    lines = res.instructions()
    assert len(lines) == len(expected), res
    for line, pattern in zip(lines, expected):
        assert re.search(pattern, line), (pattern, res)
