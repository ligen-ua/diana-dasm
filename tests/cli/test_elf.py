"""ELF files (report section "ELF")."""
import sys

import pytest

from orthia_runner import EXIT_OPEN_FAILED

pytestmark = pytest.mark.elf

DMESG_BUILD_ID = "21342db32e32e961622c0d19322ad591d56a65a8"


@pytest.fixture(scope="module")
def dmesg(data):
    return {"file": data.elf("dmesg")}


def test_lm(orthia, dmesg):
    res = orthia.run("lm", **dmesg).assert_ok()
    res.assert_line(r"^00000000`00004000\s+00000000`00014cc8\s+dmesg$")


def test_lm_analyze(orthia_full, dmesg):
    orthia_full.run("lm", **dmesg).assert_ok().assert_line(r"\bdmesg\s+analysis$")


def test_modinfo(orthia, dmesg):
    res = orthia.run("modinfo dmesg", **dmesg).assert_ok()
    res.assert_line(r"^Module: dmesg$")
    res.assert_line(rf"^Build ID: {DMESG_BUILD_ID}$")


def test_x_entrypoint(orthia, dmesg):
    res = orthia.run("x dmesg!*", **dmesg).assert_ok()
    assert res.symbols["dmesg!$entrypoint"] == [0xB0E0]


def test_u_entrypoint(orthia, dmesg):
    res = orthia.run("u dmesg!$entrypoint L4", **dmesg).assert_ok()
    assert "endbr64" in res.instructions()[0], res


@pytest.mark.parametrize("name", ["dmesg", "apt-mark"])
def test_opens(orthia, data, name):
    orthia.run("lm", file=data.elf(name)).assert_ok()


def test_cold_open_of_fresh_copy(orthia_cold, data):
    fresh = data.fresh_copy(data.elf("apt-mark"), "apt_fresh")
    res = orthia_cold.run("lm", **{"file": fresh}).assert_ok()
    res.assert_line(r"\bapt_fresh\b")


def test_unicode_path_with_spaces(orthia, data):
    path = data.fresh_copy(data.elf("dmesg"), "дмесг", subdir="dir with space/юнікод")
    res = orthia.run("lm", file=path).assert_ok()
    res.assert_line(r"\bдмесг\b")


@pytest.mark.parametrize("size", [64, 20 * 1024])
def test_truncated_elf_is_rejected(orthia, data, size):
    res = orthia.run("lm", file=data.truncated(data.elf("dmesg"), size, f"trunc_{size}"))
    assert res.code == EXIT_OPEN_FAILED, res


@pytest.mark.xfail(reason="B2: module name comes from the SHA1-keyed DB, not the opened file")
def test_renamed_copy_uses_its_own_name(orthia, data, dmesg):
    orthia.run("lm", **dmesg).assert_ok()  # make sure the DB for this SHA1 exists
    copy = data.copy(data.elf("dmesg"), "renamed/dmesg_renamed")
    res = orthia.run("lm", file=copy).assert_ok()
    res.assert_line(r"\bdmesg_renamed\b")


@pytest.mark.xfail(reason="B9: lm header truncated for short module names ('module nstatus')")
def test_lm_header(orthia, dmesg):
    orthia.run("lm", **dmesg).assert_line(r"\bmodule name\s+status\b")


@pytest.mark.skipif(sys.platform != "win32", reason="\\\\?\\ prefix is Windows-only")
@pytest.mark.xfail(reason="B17: modinfo 'Full name' shows the \\\\?\\ prefix")
def test_modinfo_full_name_has_no_long_path_prefix(orthia, dmesg):
    line = orthia.run("modinfo dmesg", **dmesg).assert_line(r"^Full name: (.*)$")
    assert not line.group(1).startswith("\\\\?\\")
