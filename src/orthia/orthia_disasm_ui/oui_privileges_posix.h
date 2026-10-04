#pragma once

#include <string>
#include <vector>

namespace orthia
{
    struct PrivilegeState
    {
        // euid was 0 at startup: process access comes from CAP_SYS_PTRACE, not from Yama
        bool startedAsRoot = false;
        // went back to the user that ran sudo/pkexec
        bool switchedUser = false;
        // CAP_SYS_PTRACE is the only capability left
        bool keptPtraceOnly = false;
        bool seccomp = false;
        std::string userName;
    };

    // Strict decimal id as found in SUDO_UID/SUDO_GID/PKEXEC_UID
    bool ParseIdText(const char* text, unsigned int* id);

    // Called once before InitAppCore, while the process has one thread.
    // Under root: switches back to the sudo/pkexec user (or stays uid 0 without one),
    // keeping only CAP_SYS_PTRACE. Throws if that can't be done: orthia never goes on as full root.
    void DropRootPrivileges();

    // Called once after InitAppCore, still with one thread: no_new_privs and a seccomp denylist.
    // Best effort: what the kernel doesn't support is skipped and reported in warnings.
    // No Landlock: it would also deny ptrace access to every process outside the sandbox.
    void ApplySandbox(std::vector<std::string>* warnings);

    const PrivilegeState& GetPrivilegeState();
}
