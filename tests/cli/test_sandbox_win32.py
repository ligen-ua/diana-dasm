"""Windows: the sandbox of normal runs (token privileges and process mitigation policies)."""
import ctypes
import os
import subprocess
import sys
import time
from ctypes import wintypes
from typing import Dict, Optional, Set

import pytest

pytestmark = [pytest.mark.process,
              pytest.mark.skipif(sys.platform != "win32", reason="Windows sandbox")]

PROCESS_QUERY_INFORMATION = 0x0400
TOKEN_QUERY = 0x0008
TOKEN_PRIVILEGES = 3  # TOKEN_INFORMATION_CLASS
# PROCESS_MITIGATION_POLICY values and the flags orthia sets in each
POLICIES = {
    "dynamic code": (2, 0x1),
    "extension points": (6, 0x1),
    "signature": (8, 0x1),
    "image load": (10, 0x7),
}
KEPT = {"SeDebugPrivilege", "SeChangeNotifyPrivilege"}


def _kernel32():
    return ctypes.WinDLL("kernel32", use_last_error=True)


def _has_mitigation_api() -> bool:
    # Windows 8+
    return hasattr(_kernel32(), "GetProcessMitigationPolicy")


def _policies(handle) -> Dict[str, int]:
    get_policy = _kernel32().GetProcessMitigationPolicy
    get_policy.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, ctypes.c_size_t]
    result = {}
    for name, (policy, _) in POLICIES.items():
        flags = wintypes.DWORD(0)
        if get_policy(handle, policy, ctypes.byref(flags), ctypes.sizeof(flags)):
            result[name] = flags.value
    return result


def _privileges(handle) -> Set[str]:
    advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)
    token = wintypes.HANDLE()
    assert advapi32.OpenProcessToken(handle, TOKEN_QUERY, ctypes.byref(token)), ctypes.get_last_error()
    try:
        size = wintypes.DWORD(0)
        advapi32.GetTokenInformation(token, TOKEN_PRIVILEGES, None, 0, ctypes.byref(size))
        buffer = ctypes.create_string_buffer(size.value)
        assert advapi32.GetTokenInformation(token, TOKEN_PRIVILEGES, buffer, size, ctypes.byref(size)), \
            ctypes.get_last_error()
        count = int.from_bytes(buffer.raw[:4], "little")
        names = set()
        for i in range(count):
            # LUID_AND_ATTRIBUTES: 4-byte packed, 12 bytes each
            luid = (ctypes.c_byte * 8).from_buffer_copy(buffer.raw, 4 + i * 12)
            name = ctypes.create_unicode_buffer(256)
            length = wintypes.DWORD(len(name))
            assert advapi32.LookupPrivilegeNameW(None, luid, name, ctypes.byref(length)), ctypes.get_last_error()
            names.add(name.value)
        return names
    finally:
        _kernel32().CloseHandle(token)


class _Ui:
    """The UI in a hidden console of its own: it needs an interactive one"""

    def __init__(self, exe, home, symbols, *args: str):
        env = dict(os.environ, ORTHIA_HOME=str(home), ORTHIA_SYMBOL_PATH=str(symbols))
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0  # SW_HIDE
        self.proc = subprocess.Popen([str(exe), *args], env=env, startupinfo=startup,
                                     creationflags=subprocess.CREATE_NEW_CONSOLE)
        kernel32 = _kernel32()
        kernel32.OpenProcess.restype = wintypes.HANDLE
        self.handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION, False, self.proc.pid)
        assert self.handle, ctypes.get_last_error()

    def close(self):
        _kernel32().CloseHandle(wintypes.HANDLE(self.handle))
        self.proc.kill()
        self.proc.wait(timeout=10)

    def wait_for_policies(self, timeout: float) -> Optional[Dict[str, int]]:
        """The policies once dynamic code is prohibited (the sandbox is in place), or the last seen"""
        deadline = time.time() + timeout
        policies = None
        while time.time() < deadline:
            assert self.proc.poll() is None, f"orthia exited with {self.proc.returncode}"
            policies = _policies(self.handle)
            if policies.get("dynamic code", 0) & 1:
                break
            time.sleep(0.2)
        return policies


def test_normal_run_is_sandboxed(orthia_exe, tmp_path, empty_symbols):
    ui = _Ui(orthia_exe, tmp_path, empty_symbols)
    try:
        if _has_mitigation_api():
            policies = ui.wait_for_policies(timeout=15)
            for name, (_, flags) in POLICIES.items():
                assert policies.get(name, 0) & flags == flags, (name, policies)
        else:
            time.sleep(3)
        left = _privileges(ui.handle)
        assert left <= KEPT, left
    finally:
        ui.close()


def test_no_sandbox(orthia_exe, tmp_path, empty_symbols):
    ui = _Ui(orthia_exe, tmp_path, empty_symbols, "--no-sandbox")
    try:
        if _has_mitigation_api():
            # nothing marks the end of the startup without a sandbox: give it the time a sandbox would take
            policies = ui.wait_for_policies(timeout=3)
            assert not policies.get("dynamic code", 0) & 1, policies
            assert not policies.get("signature", 0) & 1, policies
        else:
            time.sleep(3)
        # every token has more than these two (SeShutdownPrivilege, SeIncreaseWorkingSetPrivilege...)
        assert _privileges(ui.handle) - KEPT, "privileges were removed"
    finally:
        ui.close()
