#include "oui_privileges_posix.h"
#include "orthia_log.h"

#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <linux/audit.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <sched.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>

// Raw syscalls and kernel constants only: no libcap/libseccomp dependency,
// and the constants of newer kernels are defined here when the headers are older.

#ifndef PR_CAP_AMBIENT
#define PR_CAP_AMBIENT 47
#define PR_CAP_AMBIENT_CLEAR_ALL 4
#endif

// linux/securebits.h
#define ORTHIA_SECBIT_NOROOT                       (1 << 0)
#define ORTHIA_SECBIT_NOROOT_LOCKED                (1 << 1)
#define ORTHIA_SECBIT_NO_SETUID_FIXUP_LOCKED       (1 << 3)
#define ORTHIA_SECBIT_KEEP_CAPS_LOCKED             (1 << 5)
#define ORTHIA_SECBIT_NO_CAP_AMBIENT_RAISE         (1 << 6)
#define ORTHIA_SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED  (1 << 7)

#ifndef SECCOMP_RET_KILL_PROCESS
#define SECCOMP_RET_KILL_PROCESS SECCOMP_RET_KILL
#endif

#if defined(__x86_64__)
#define ORTHIA_AUDIT_ARCH AUDIT_ARCH_X86_64
#elif defined(__aarch64__)
#define ORTHIA_AUDIT_ARCH AUDIT_ARCH_AARCH64
#endif

namespace orthia
{
namespace
{
    PrivilegeState g_state;

    std::runtime_error Error(const std::string& what, int error)
    {
        return std::runtime_error("can't drop root privileges: " + what + ": " + strerror(error) +
                                  " (--no-privilege-drop runs orthia with full root rights)");
    }

    void Check(int result, const char* what)
    {
        if (result != 0)
        {
            throw Error(what, errno);
        }
    }

    // capset and seccomp apply to the calling thread only
    int CountThreads()
    {
        DIR* dir = opendir("/proc/self/task");
        if (!dir)
        {
            return -1;
        }
        int count = 0;
        while (struct dirent* entry = readdir(dir))
        {
            if (entry->d_name[0] != '.')
            {
                ++count;
            }
        }
        closedir(dir);
        return count;
    }

    bool CapGet(__user_cap_data_struct (&data)[2])
    {
        __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
        memset(data, 0, sizeof(data));
        return syscall(SYS_capget, &header, data) == 0;
    }

    // permitted = effective = {CAP_SYS_PTRACE}, inheritable = {}
    void SetPtraceOnly()
    {
        __user_cap_header_struct header = { _LINUX_CAPABILITY_VERSION_3, 0 };
        __user_cap_data_struct data[2];
        memset(data, 0, sizeof(data));
        data[CAP_TO_INDEX(CAP_SYS_PTRACE)].permitted = CAP_TO_MASK(CAP_SYS_PTRACE);
        data[CAP_TO_INDEX(CAP_SYS_PTRACE)].effective = CAP_TO_MASK(CAP_SYS_PTRACE);
        Check((int)syscall(SYS_capset, &header, data), "capset");
    }

    void DropBoundingSet()
    {
        for (int cap = 0; cap < 64; ++cap)
        {
            if (prctl(PR_CAPBSET_READ, cap, 0, 0, 0) < 0)
            {
                // past the last capability of this kernel
                break;
            }
            if (cap != CAP_SYS_PTRACE)
            {
                Check(prctl(PR_CAPBSET_DROP, cap, 0, 0, 0), "PR_CAPBSET_DROP");
            }
        }
    }

    void VerifyDropped(uid_t uid, gid_t gid)
    {
        uid_t ruid, euid, suid;
        gid_t rgid, egid, sgid;
        if (getresuid(&ruid, &euid, &suid) || getresgid(&rgid, &egid, &sgid) ||
            ruid != uid || euid != uid || suid != uid ||
            rgid != gid || egid != gid || sgid != gid)
        {
            throw Error("user ids did not change", EPERM);
        }
        __user_cap_data_struct data[2];
        if (!CapGet(data))
        {
            throw Error("capget", errno);
        }
        for (int i = 0; i < 2; ++i)
        {
            const uint32_t expected = i == CAP_TO_INDEX(CAP_SYS_PTRACE) ? CAP_TO_MASK(CAP_SYS_PTRACE) : 0;
            if (data[i].permitted != expected || data[i].effective != expected || data[i].inheritable)
            {
                throw Error("capabilities other than CAP_SYS_PTRACE are left", EPERM);
            }
        }
        for (int cap = 0; cap < 64; ++cap)
        {
            const int inSet = prctl(PR_CAPBSET_READ, cap, 0, 0, 0);
            if (inSet < 0)
            {
                break;
            }
            if (inSet && cap != CAP_SYS_PTRACE)
            {
                throw Error("the bounding set keeps other capabilities", EPERM);
            }
        }
        if (uid != 0 && setuid(0) == 0)
        {
            throw Error("root can be regained", EPERM);
        }
    }

    // The user that ran sudo/pkexec, or nullptr when there is none (a root login, su)
    const passwd* FindInvokingUser()
    {
        for (const char* name : { "SUDO_UID", "PKEXEC_UID" })
        {
            const char* text = getenv(name);
            if (!text)
            {
                continue;
            }
            unsigned int uid = 0;
            if (!ParseIdText(text, &uid))
            {
                throw Error(std::string("bad ") + name + " \"" + text + "\"", EINVAL);
            }
            if (uid == 0)
            {
                return nullptr;
            }
            errno = 0;
            const passwd* user = getpwuid((uid_t)uid);
            if (!user)
            {
                throw Error(std::string("no user for ") + name + "=" + text, errno ? errno : ENOENT);
            }
            return user;
        }
        return nullptr;
    }

#ifdef ORTHIA_AUDIT_ARCH
    // What CAP_SYS_PTRACE would allow besides reading memory, and the usual kernel attack surface
    const int g_deniedSyscalls[] = {
        __NR_ptrace,
        __NR_process_vm_writev,
#ifdef __NR_pidfd_getfd
        __NR_pidfd_getfd,
#endif
#ifdef __NR_process_madvise
        __NR_process_madvise,
#endif
        __NR_kcmp,
        __NR_mount,
        __NR_umount2,
        __NR_pivot_root,
        __NR_chroot,
        __NR_kexec_load,
#ifdef __NR_kexec_file_load
        __NR_kexec_file_load,
#endif
        __NR_init_module,
        __NR_finit_module,
        __NR_delete_module,
        __NR_bpf,
        __NR_perf_event_open,
        __NR_userfaultfd,
        __NR_keyctl,
        __NR_add_key,
        __NR_request_key,
        __NR_unshare,
        __NR_setns,
        __NR_open_by_handle_at,
#ifdef __NR_io_uring_setup
        __NR_io_uring_setup,
        __NR_io_uring_enter,
        __NR_io_uring_register,
#endif
#ifdef __NR_open_tree
        __NR_open_tree,
        __NR_move_mount,
        __NR_fsopen,
        __NR_fsconfig,
        __NR_fsmount,
        __NR_fspick,
#endif
        __NR_swapon,
        __NR_swapoff,
        __NR_reboot,
        __NR_acct,
        __NR_syslog,
#ifdef __NR_iopl
        __NR_iopl,
        __NR_ioperm,
#endif
    };

    // Namespaces would give a fresh set of capabilities inside them
    const uint32_t g_cloneNamespaceFlags = CLONE_NEWNS | CLONE_NEWCGROUP | CLONE_NEWUTS | CLONE_NEWIPC |
                                           CLONE_NEWUSER | CLONE_NEWPID | CLONE_NEWNET
#ifdef CLONE_NEWTIME
                                           | CLONE_NEWTIME
#endif
                                           ;

    bool ApplySeccomp(std::vector<std::string>* warnings)
    {
        std::vector<sock_filter> filter;
        auto add = [&](sock_filter item) { filter.push_back(item); };
        const uint32_t denied = SECCOMP_RET_ERRNO | (EPERM & SECCOMP_RET_DATA);

        add(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, arch)));
        add(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, ORTHIA_AUDIT_ARCH, 1, 0));
        // another ABI (i386, x32) has other syscall numbers: the denylist would not apply
        add(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
        add(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, nr)));
#ifdef __x86_64__
        add(BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, 0x40000000 /* __X32_SYSCALL_BIT */, 0, 1));
        add(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS));
#endif
        for (int nr : g_deniedSyscalls)
        {
            add(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, (uint32_t)nr, 0, 1));
            add(BPF_STMT(BPF_RET | BPF_K, denied));
        }
#ifdef __NR_clone3
        // its flags are in memory, out of the filter's reach: glibc falls back to clone on ENOSYS
        add(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone3, 0, 1));
        add(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | (ENOSYS & SECCOMP_RET_DATA)));
#endif
        // last: it replaces the syscall number with the flags
        add(BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone, 0, 3));
        add(BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(seccomp_data, args[0])));
        add(BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, g_cloneNamespaceFlags, 0, 1));
        add(BPF_STMT(BPF_RET | BPF_K, denied));
        add(BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW));

        sock_fprog program = { (unsigned short)filter.size(), filter.data() };
        if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program, 0, 0) != 0)
        {
            warnings->push_back(std::string("seccomp is not available: ") + strerror(errno));
            return false;
        }
        return true;
    }
#else
    bool ApplySeccomp(std::vector<std::string>* warnings)
    {
        warnings->push_back("seccomp: no syscall filter for this architecture");
        return false;
    }
#endif
}

    bool ParseIdText(const char* text, unsigned int* id)
    {
        if (!text || !*text || strlen(text) > 10)
        {
            return false;
        }
        unsigned long long value = 0;
        for (const char* p = text; *p; ++p)
        {
            if (*p < '0' || *p > '9')
            {
                return false;
            }
            value = value * 10 + (unsigned)(*p - '0');
        }
        // (uid_t)-1 means "unchanged" to setresuid
        if (value >= 0xFFFFFFFFull)
        {
            return false;
        }
        *id = (unsigned int)value;
        return true;
    }

    void DropRootPrivileges()
    {
        if (geteuid() != 0)
        {
            return;
        }
        if (getuid() != 0)
        {
            // setuid-root install: anyone could use it to read any process
            throw std::runtime_error("orthia is installed setuid root, which is not supported: run it with sudo");
        }
        if (CountThreads() != 1)
        {
            throw Error("more than one thread is running", EBUSY);
        }
        g_state.startedAsRoot = true;

        // the lookups load NSS modules: while still root
        const passwd* user = FindInvokingUser();
        uid_t uid = 0;
        gid_t gid = 0;
        std::string userName = "root";
        std::string home;
        if (user)
        {
            uid = user->pw_uid;
            gid = user->pw_gid;
            userName = user->pw_name;
            home = user->pw_dir ? user->pw_dir : "";
            Check(initgroups(user->pw_name, gid), "initgroups");
        }

        // no capabilities from executing anything as uid 0, no raising ambient ones
        int securebits = ORTHIA_SECBIT_NOROOT | ORTHIA_SECBIT_NOROOT_LOCKED |
                         ORTHIA_SECBIT_NO_CAP_AMBIENT_RAISE | ORTHIA_SECBIT_NO_CAP_AMBIENT_RAISE_LOCKED;
        if (!user)
        {
            // staying uid 0: changing the uid later still clears the capabilities
            securebits |= ORTHIA_SECBIT_NO_SETUID_FIXUP_LOCKED | ORTHIA_SECBIT_KEEP_CAPS_LOCKED;
        }
        Check(prctl(PR_SET_SECUREBITS, securebits, 0, 0, 0), "PR_SET_SECUREBITS");
        DropBoundingSet();
        // older kernels have no ambient set
        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0, 0, 0) != 0 && errno != EINVAL)
        {
            throw Error("PR_CAP_AMBIENT_CLEAR_ALL", errno);
        }

        if (user)
        {
            Check(prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0), "PR_SET_KEEPCAPS");
            Check(setresgid(gid, gid, gid), "setresgid");
            Check(setresuid(uid, uid, uid), "setresuid");
            Check(prctl(PR_SET_KEEPCAPS, 0, 0, 0, 0), "PR_SET_KEEPCAPS");
        }
        SetPtraceOnly();
        // the user must not attach to orthia (or write its /proc/self/mem) and take CAP_SYS_PTRACE over
        Check(prctl(PR_SET_DUMPABLE, 0, 0, 0, 0), "PR_SET_DUMPABLE");
        Check(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0), "PR_SET_NO_NEW_PRIVS");
        VerifyDropped(uid, gid);

        if (user)
        {
            // the data folder and ~ in the symbol paths are the user's, not root's
            if (!home.empty())
            {
                setenv("HOME", home.c_str(), 1);
            }
            setenv("USER", userName.c_str(), 1);
            setenv("LOGNAME", userName.c_str(), 1);
            g_state.switchedUser = true;
        }
        g_state.keptPtraceOnly = true;
        g_state.userName = userName;
        ORTHIA_DEV_LOG(orthia::LogSeverity::Info, "Dropped root privileges, running as " + userName + " with CAP_SYS_PTRACE");
    }

    void ApplySandbox(std::vector<std::string>* warnings)
    {
        if (CountThreads() != 1)
        {
            warnings->push_back("sandbox: more than one thread is running, skipped");
            return;
        }
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0)
        {
            warnings->push_back(std::string("PR_SET_NO_NEW_PRIVS failed: ") + strerror(errno));
            return;
        }
        g_state.seccomp = ApplySeccomp(warnings);
    }

    const PrivilegeState& GetPrivilegeState()
    {
        return g_state;
    }
}
