#include "oui_sandbox_win32.h"
#include "orthia_utils.h"
#include "orthia_log.h"

// The build targets XP (v141_xp, 7.1A SDK): everything newer is looked up at runtime
// and its constants are defined here.

#ifndef SE_PRIVILEGE_REMOVED
#define SE_PRIVILEGE_REMOVED 0x00000004L
#endif

namespace orthia
{
namespace
{
    // PROCESS_MITIGATION_POLICY values (Windows 8+ SDK)
    const int g_processDynamicCodePolicy = 2;
    const int g_processExtensionPointDisablePolicy = 6;
    const int g_processSignaturePolicy = 8;
    const int g_processImageLoadPolicy = 10;

    // The first bits of the policy structures, each of which is one DWORD of flags
    const DWORD g_prohibitDynamicCode = 1 << 0;
    const DWORD g_disableExtensionPoints = 1 << 0;
    const DWORD g_microsoftSignedOnly = 1 << 0;
    const DWORD g_noRemoteImages = 1 << 0;
    const DWORD g_noLowMandatoryLabelImages = 1 << 1;
    const DWORD g_preferSystem32Images = 1 << 2;

    typedef BOOL(WINAPI* SetProcessMitigationPolicy_type)(int policy, PVOID buffer, SIZE_T length);

    std::string ErrorText(DWORD error)
    {
        return "error " + std::to_string(error);
    }

    std::vector<char> QueryTokenInformation(HANDLE hToken, TOKEN_INFORMATION_CLASS infoClass, DWORD* error)
    {
        DWORD size = 0;
        GetTokenInformation(hToken, infoClass, 0, 0, &size);
        if (!size)
        {
            *error = GetLastError();
            return std::vector<char>();
        }
        std::vector<char> buffer(size);
        if (!GetTokenInformation(hToken, infoClass, buffer.data(), size, &size))
        {
            *error = GetLastError();
            return std::vector<char>();
        }
        *error = 0;
        return buffer;
    }

    bool IsKept(const LUID& luid, const LUID& debug, const LUID& changeNotify)
    {
        auto equal = [](const LUID& a, const LUID& b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; };
        return equal(luid, debug) || equal(luid, changeNotify);
    }

    bool AdjustOne(HANDLE hToken, const LUID& luid, DWORD attributes)
    {
        TOKEN_PRIVILEGES privileges = {};
        privileges.PrivilegeCount = 1;
        privileges.Privileges[0].Luid = luid;
        privileges.Privileges[0].Attributes = attributes;
        // succeeds with ERROR_NOT_ALL_ASSIGNED when nothing changed
        return AdjustTokenPrivileges(hToken, FALSE, &privileges, 0, 0, 0) && GetLastError() == ERROR_SUCCESS;
    }

    // SeDebugPrivilege is the Windows counterpart of CAP_SYS_PTRACE: kept as it is, not enabled.
    // SeChangeNotifyPrivilege is the traverse checking bypass that paths rely on.
    void RemovePrivileges(std::vector<std::string>* warnings)
    {
        HANDLE hToken = 0;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            warnings->push_back("sandbox: can't open the process token: " + ErrorText(GetLastError()));
            return;
        }
        CHandleGuard tokenGuard(hToken);

        LUID debug = {}, changeNotify = {};
        if (!LookupPrivilegeValueW(0, SE_DEBUG_NAME, &debug) ||
            !LookupPrivilegeValueW(0, SE_CHANGE_NOTIFY_NAME, &changeNotify))
        {
            warnings->push_back("sandbox: can't look up privileges: " + ErrorText(GetLastError()));
            return;
        }

        DWORD error = 0;
        std::vector<char> buffer = QueryTokenInformation(hToken, TokenPrivileges, &error);
        if (buffer.empty())
        {
            warnings->push_back("sandbox: can't read the token privileges: " + ErrorText(error));
            return;
        }
        const TOKEN_PRIVILEGES* privileges = (const TOKEN_PRIVILEGES*)buffer.data();
        bool removeSupported = true;
        for (DWORD i = 0; i < privileges->PrivilegeCount; ++i)
        {
            const LUID luid = privileges->Privileges[i].Luid;
            if (IsKept(luid, debug, changeNotify))
            {
                continue;
            }
            if (removeSupported && AdjustOne(hToken, luid, SE_PRIVILEGE_REMOVED))
            {
                continue;
            }
            if (removeSupported && GetLastError() == ERROR_INVALID_PARAMETER)
            {
                // XP before SP2: no removal, only disabling
                ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "sandbox: privileges can't be removed here, disabling them");
                removeSupported = false;
            }
            if (!removeSupported && AdjustOne(hToken, luid, 0))
            {
                continue;
            }
            if (!removeSupported && GetLastError() == ERROR_NOT_ALL_ASSIGNED)
            {
                // already disabled
                continue;
            }
            warnings->push_back("sandbox: can't remove a privilege: " + ErrorText(GetLastError()));
        }

        // what is left must be the kept ones (or disabled ones, without removal)
        buffer = QueryTokenInformation(hToken, TokenPrivileges, &error);
        if (buffer.empty())
        {
            warnings->push_back("sandbox: can't read the token privileges: " + ErrorText(error));
            return;
        }
        privileges = (const TOKEN_PRIVILEGES*)buffer.data();
        for (DWORD i = 0; i < privileges->PrivilegeCount; ++i)
        {
            const LUID_AND_ATTRIBUTES& item = privileges->Privileges[i];
            if (IsKept(item.Luid, debug, changeNotify))
            {
                continue;
            }
            if (removeSupported || (item.Attributes & SE_PRIVILEGE_ENABLED))
            {
                warnings->push_back("sandbox: privileges other than SeDebugPrivilege are left");
                return;
            }
        }
    }

    // Returns true if the policy is in place
    bool SetPolicy(SetProcessMitigationPolicy_type setPolicy, int policy, DWORD flags, const char* name,
                   std::vector<std::string>* warnings)
    {
        if (setPolicy(policy, &flags, sizeof(flags)))
        {
            return true;
        }
        const DWORD error = GetLastError();
        if (error == ERROR_INVALID_PARAMETER || error == ERROR_NOT_SUPPORTED)
        {
            // newer than this Windows version
            ORTHIA_DEV_LOG(orthia::LogSeverity::Info, std::string("sandbox: no ") + name + " policy on this Windows version");
            return false;
        }
        warnings->push_back(std::string("sandbox: can't set the ") + name + " policy: " + ErrorText(error));
        return false;
    }

    // Returns true if the signature policy is in place
    bool SetMitigationPolicies(std::vector<std::string>* warnings)
    {
        auto setPolicy = (SetProcessMitigationPolicy_type)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                                                                          "SetProcessMitigationPolicy");
        if (!setPolicy)
        {
            // before Windows 8
            ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "sandbox: no process mitigation policies on this Windows version");
            return false;
        }
        // nothing in orthia generates code at runtime (only the tests, which run before this)
        SetPolicy(setPolicy, g_processDynamicCodePolicy, g_prohibitDynamicCode, "dynamic code", warnings);
        // AppInit DLLs, legacy IMEs and window hooks
        SetPolicy(setPolicy, g_processExtensionPointDisablePolicy, g_disableExtensionPoints, "extension point", warnings);
        SetPolicy(setPolicy, g_processImageLoadPolicy,
                  g_noRemoteImages | g_noLowMandatoryLabelImages | g_preferSystem32Images, "image load", warnings);
        // a DLL planted next to orthia.exe, orthia_proc_win32.dll included
        return SetPolicy(setPolicy, g_processSignaturePolicy, g_microsoftSignedOnly, "signature", warnings);
    }
}

    bool IsElevated()
    {
        HANDLE hToken = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        {
            CHandleGuard tokenGuard(hToken);
            TOKEN_ELEVATION elevation = {};
            DWORD size = 0;
            if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size))
            {
                return elevation.TokenIsElevated != 0;
            }
        }
        // XP: no TokenElevation, an administrator is always "elevated"
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        PSID administrators = 0;
        if (!AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                      0, 0, 0, 0, 0, 0, &administrators))
        {
            return false;
        }
        BOOL member = FALSE;
        if (!CheckTokenMembership(0, administrators, &member))
        {
            member = FALSE;
        }
        FreeSid(administrators);
        return member != FALSE;
    }

    bool ApplySandbox(std::vector<std::string>* warnings)
    {
        RemovePrivileges(warnings);
        return SetMitigationPolicies(warnings);
    }
}
