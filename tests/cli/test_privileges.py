"""Linux: what is left of root after start (sudo, or a root login), and the sandbox of normal runs.

The root cases need root or passwordless sudo; they are skipped otherwise.
"""
import os
import pty
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional

import pytest

pytestmark = [pytest.mark.process,
              pytest.mark.skipif(sys.platform != "linux", reason="Linux privilege handling")]

CAP_SYS_PTRACE = 1 << 19


def _can_sudo() -> bool:
    if os.geteuid() == 0:
        return False
    if not shutil.which("sudo"):
        return False
    return subprocess.run(["sudo", "-n", "true"], capture_output=True).returncode == 0


needs_sudo = pytest.mark.skipif(not _can_sudo(), reason="needs passwordless sudo, as a normal user")


def _sudo(exe: Path, args: List[str], env: Dict[str, str], keep_sudo_uid: bool = True) -> List[str]:
    """sudo resets the environment: pass what orthia needs explicitly"""
    prefix = ["sudo", "-n", "env"]
    if not keep_sudo_uid:
        # what a root login looks like
        prefix += ["-u", "SUDO_UID", "-u", "SUDO_GID", "-u", "SUDO_USER"]
    return prefix + [f"{k}={v}" for k, v in env.items()] + [str(exe)] + args


def _status(pid: int) -> Dict[str, str]:
    result = {}
    for line in Path(f"/proc/{pid}/status").read_text().splitlines():
        key, _, value = line.partition(":")
        result[key] = value.strip()
    return result


def _ui_status(command: List[str]) -> Dict[str, str]:
    """Starts the UI on a pty (it needs a console), reads its /proc status, stops it"""
    master, slave = pty.openpty()
    proc = subprocess.Popen(command, stdin=slave, stdout=slave, stderr=slave, start_new_session=True)
    os.close(slave)
    try:
        # orthia is a child of sudo (or sudo's monitor process): the newest process named orthia;
        # seccomp is the last step of the drop
        deadline = time.time() + 10
        status = None
        while time.time() < deadline and not (status and status.get("Seccomp") == "2"):
            time.sleep(0.2)
            found = subprocess.run(["pgrep", "-n", "-x", "orthia"], capture_output=True, text=True)
            if found.returncode == 0:
                try:
                    status = _status(int(found.stdout.split()[0]))
                except FileNotFoundError:
                    pass
        assert status is not None, "the UI did not start"
        return status
    finally:
        if command[0] == "sudo":
            subprocess.run(["sudo", "-n", "pkill", "-x", "orthia"], capture_output=True)
        else:
            proc.terminate()
        try:
            proc.wait(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
        os.close(master)


def _caps(status: Dict[str, str], name: str) -> int:
    return int(status[name], 16)


@pytest.fixture
def root_home():
    """A data folder root can write without CAP_DAC_OVERRIDE (pytest's tmp is 0700 and the user's)"""
    path = subprocess.run(["sudo", "-n", "mktemp", "-d"], capture_output=True, text=True,
                          check=True).stdout.strip()
    yield Path(path)
    subprocess.run(["sudo", "-n", "rm", "-rf", path], check=True)


def test_normal_run_is_sandboxed(orthia_exe, tmp_path, empty_symbols):
    if os.geteuid() == 0:
        pytest.skip("run as a normal user")
    status = _ui_status(["env", f"ORTHIA_HOME={tmp_path}", f"ORTHIA_SYMBOL_PATH={empty_symbols}",
                         str(orthia_exe)])
    assert status["NoNewPrivs"] == "1", status
    assert status["Seccomp"] == "2", status
    assert _caps(status, "CapEff") == 0, status


@needs_sudo
def test_sudo_goes_back_to_the_user_with_ptrace_only(orthia_exe, tmp_path, empty_symbols):
    status = _ui_status(_sudo(orthia_exe, [], {"ORTHIA_HOME": tmp_path,
                                               "ORTHIA_SYMBOL_PATH": empty_symbols}))
    uid, gid = str(os.getuid()), str(os.getgid())
    assert status["Uid"].split() == [uid] * 4, status
    assert status["Gid"].split() == [gid] * 4, status
    for name in ("CapPrm", "CapEff", "CapBnd"):
        assert _caps(status, name) == CAP_SYS_PTRACE, (name, status)
    assert _caps(status, "CapInh") == 0 and _caps(status, "CapAmb") == 0, status
    assert status["NoNewPrivs"] == "1", status
    assert status["Seccomp"] == "2", status


@needs_sudo
def test_sudo_reads_root_process_and_writes_as_the_user(orthia_exe, tmp_path, empty_symbols):
    home = tmp_path / "home"
    res = subprocess.run(_sudo(orthia_exe, ["--pid", "1", "--cmd", "lm"],
                               {"ORTHIA_HOME": home, "ORTHIA_SYMBOL_PATH": empty_symbols}),
                         capture_output=True, text=True, timeout=120)
    assert res.returncode == 0, res
    assert re.search(r"^Opening: \"\[1\] ", res.stdout + res.stderr, re.M), res
    owners = {p.stat().st_uid for p in [home, *home.rglob("*")]}
    assert owners == {os.getuid()}, owners


@needs_sudo
def test_root_login_keeps_uid_0_with_ptrace_only(orthia_exe, root_home, empty_symbols):
    env = {"ORTHIA_HOME": root_home, "ORTHIA_SYMBOL_PATH": empty_symbols}
    status = _ui_status(_sudo(orthia_exe, [], env, keep_sudo_uid=False))
    assert status["Uid"].split() == ["0"] * 4, status
    for name in ("CapPrm", "CapEff", "CapBnd"):
        assert _caps(status, name) == CAP_SYS_PTRACE, (name, status)
    assert status["NoNewPrivs"] == "1", status

    # another user's process: CAP_SYS_PTRACE is enough, without CAP_DAC_OVERRIDE
    sleeper = subprocess.Popen(["sleep", "60"])
    try:
        res = subprocess.run(_sudo(orthia_exe, ["--pid", str(sleeper.pid), "--cmd", "lm"], env,
                                   keep_sudo_uid=False),
                             capture_output=True, text=True, timeout=120)
        assert res.returncode == 0, res
    finally:
        sleeper.kill()
