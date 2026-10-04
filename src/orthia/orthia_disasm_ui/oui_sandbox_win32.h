#pragma once

#include <string>
#include <vector>

namespace orthia
{
    // Elevated admin token (TokenElevation), or a member of Administrators on XP
    bool IsElevated();

    // Called once after InitAppCore, DianaWin32_Init and the host exe setup, before any process is opened.
    // Removes every token privilege except SeDebugPrivilege and SeChangeNotifyPrivilege,
    // then sets the process mitigation policies this Windows version has.
    // Best effort: what Windows doesn't support is skipped silently, failures are reported in warnings.
    // Returns true if only Microsoft-signed DLLs load from now on (Windows 10+),
    // which keeps orthia_proc_win32.dll out as well: --no-sandbox for it.
    bool ApplySandbox(std::vector<std::string>* warnings);
}
