#include "oui_processes.h"
#include "oui_window_thread.h"
#include "oui_base_posix.h"
#include "orthia_process_adapter.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_log.h"
#include "orthia_utils.h"

extern "C"
{
#include "diana_processor/diana_processor_context.h"
}

#include <sys/uio.h>     // process_vm_readv
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <map>

namespace oui
{
    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------

    static bool IsDigitString(const char* s)
    {
        if (!s || !*s) return false;
        for (; *s; ++s)
            if (*s < '0' || *s > '9') return false;
        return true;
    }

    // Returns 0 on success, errno-style error on failure.
    static int ReadProcessMemoryPosix(pid_t pid,
        unsigned long long offset,
        void* pBuffer,
        size_t size)
    {
        if (!size) return 0;

        struct iovec local  = { pBuffer, size };
        struct iovec remote = { reinterpret_cast<void*>(offset), size };

        ssize_t n = process_vm_readv(pid, &local, 1, &remote, 1, 0);
        if (n < 0)
            return errno;
        if ((size_t)n != size)
        {
            // partial — fill the rest with zero
            memset(static_cast<char*>(pBuffer) + n, 0, size - n);
        }
        return 0;
    }

    // Detect bitness by reading the ELF header of /proc/<pid>/exe.
    // Returns pointer size in bytes (4 or 8). Returns 8 on error (safe default).
    static int DetectPointerSize(pid_t pid)
    {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);

        int fd = open(path, O_RDONLY);
        if (fd < 0) return 8;

        unsigned char ident[5] = {};
        ssize_t r = read(fd, ident, sizeof(ident));
        close(fd);

        if (r < 5) return 8;
        // EI_MAG0..3 = 0x7f 'E' 'L' 'F'
        if (ident[0] != 0x7f || ident[1] != 'E' || ident[2] != 'L' || ident[3] != 'F')
            return 8;
        // EI_CLASS: 1=32bit, 2=64bit
        return (ident[4] == 1) ? 4 : 8;
    }

    // Read the short name of a process from /proc/<pid>/comm
    static std::string ReadProcessComm(pid_t pid)
    {
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/comm", (int)pid);
        std::ifstream f(path);
        std::string name;
        std::getline(f, name);
        return name;
    }

    // -------------------------------------------------------------------------
    // PosixThread
    // -------------------------------------------------------------------------

    class CPosixThread : public IThread
    {
        pid_t m_pid;
        pid_t m_tid;
    public:
        CPosixThread(pid_t pid, pid_t tid)
            : m_pid(pid), m_tid(tid)
        {}

        int GetInfo(ThreadInfo& info) const override
        {
            info.tid = (unsigned long long)m_tid;
            info.startAddress = 0;

            // Read start time from /proc/<pid>/task/<tid>/stat field 22 (starttime)
            char path[128];
            snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", (int)m_pid, (int)m_tid);
            std::ifstream f(path);
            if (f.is_open())
            {
                std::string line;
                std::getline(f, line);
                // skip past the closing ')' of the comm field
                auto pos = line.rfind(')');
                if (pos != std::string::npos)
                {
                    std::istringstream ss(line.substr(pos + 1));
                    std::string state;
                    int ppid, pgrp, session, tty, tpgid;
                    unsigned flags, minflt, cminflt, majflt, cmajflt;
                    unsigned long utime, stime, cutime, cstime, priority, nice, numthreads, itrealvalue;
                    unsigned long long starttime = 0;
                    ss >> state >> ppid >> pgrp >> session >> tty >> tpgid
                       >> flags >> minflt >> cminflt >> majflt >> cmajflt
                       >> utime >> stime >> cutime >> cstime >> priority >> nice
                       >> numthreads >> itrealvalue >> starttime;
                    // starttime is in clock ticks since boot — no direct conversion available, leave default
                }
            }
            return 0;
        }

        int CaptureStack(int /*framesCount*/, std::vector<orthia::Address_type>& tstack) const override
        {
            // Stack capture requires ptrace — deferred for first iteration
            tstack.clear();
            return ENOTSUP;
        }
    };

    // -------------------------------------------------------------------------
    // PosixThreadEnumerator
    // -------------------------------------------------------------------------

    class CPosixThreadEnumerator : public IThreadEnumerator, Noncopyable
    {
        pid_t m_pid;
        DIR*  m_dir = nullptr;
    public:
        CPosixThreadEnumerator(pid_t pid)
            : m_pid(pid)
        {
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d/task", (int)pid);
            m_dir = opendir(path);
        }
        ~CPosixThreadEnumerator()
        {
            if (m_dir) closedir(m_dir);
        }

        std::tuple<int, std::shared_ptr<IThread>> GetNextThread() override
        {
            if (!m_dir)
                return { ENOENT, nullptr };

            while (true)
            {
                errno = 0;
                struct dirent* entry = readdir(m_dir);
                if (!entry)
                    return { 0, nullptr }; // end of list

                if (!IsDigitString(entry->d_name))
                    continue;

                pid_t tid = (pid_t)atoi(entry->d_name);
                return { 0, std::make_shared<CPosixThread>(m_pid, tid) };
            }
        }
    };

    // -------------------------------------------------------------------------
    // Module map entry from /proc/<pid>/maps
    // -------------------------------------------------------------------------

    struct MapsEntry
    {
        unsigned long long startAddr = 0;
        unsigned long long endAddr   = 0;
        unsigned long long offset    = 0;
        std::string        path;
        bool               isExec    = false;
    };

    static std::vector<MapsEntry> ParseMaps(pid_t pid)
    {
        char mapsPath[64];
        snprintf(mapsPath, sizeof(mapsPath), "/proc/%d/maps", (int)pid);

        std::vector<MapsEntry> entries;
        std::ifstream f(mapsPath);
        std::string line;
        while (std::getline(f, line))
        {
            // format: startAddr-endAddr perms offset dev inode pathname
            MapsEntry e;
            char perms[8] = {};
            char path[512] = {};
            unsigned long long dev1, dev2, inode;
            int matched = sscanf(line.c_str(),
                "%llx-%llx %7s %llx %llx:%llx %llu %511s",
                &e.startAddr, &e.endAddr,
                perms,
                &e.offset,
                &dev1, &dev2, &inode,
                path);
            if (matched >= 4)
            {
                e.isExec = (perms[2] == 'x');
                if (matched >= 8 && path[0] == '/')
                    e.path = path;
                entries.push_back(e);
            }
        }
        return entries;
    }

    // -------------------------------------------------------------------------
    // CPosixProcess
    // -------------------------------------------------------------------------

    class CPosixProcess : public IProcess,
                          public std::enable_shared_from_this<CPosixProcess>,
                          Noncopyable
    {
        pid_t  m_pid;
        String m_name;
        bool   m_is32bit;
        unsigned long long m_pos = 0;

    public:
        CPosixProcess(pid_t pid, const String& name, bool is32bit)
            : m_pid(pid), m_name(name), m_is32bit(is32bit)
        {}

        // IFile
        std::tuple<int, unsigned long long> GetSizeInBytes() const override
        {
            unsigned long long maxAddr = m_is32bit
                ? (unsigned long long)0xFFFFFFFFULL
                : (unsigned long long)0xFFFFFFFFFFFFFFFFULL;
            return { 0, maxAddr };
        }

        oui::String GetFullFileName() const override    { return m_name; }
        oui::String GetFullFileNameForUI() const override { return m_name; }

        int ReadExact(std::shared_ptr<BaseOperation> operation,
            unsigned long long offset, size_t size,
            std::vector<char>& buffer) override
        {
            if (offset != (unsigned long long)(-1))
                m_pos = offset;

            buffer.resize(size);
            int err = ReadExactEx2(m_pos, buffer.data(), size);
            if (!err) m_pos += size;
            return err;
        }

        // IProcess
        int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) override
        {
            return ReadProcessMemoryPosix(m_pid, offset, pBuffer, size);
        }

        orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) override
        {
            std::vector<char> buffer(size);
            if (ReadProcessMemoryPosix(m_pid, offset, buffer.data(), size))
                return orthia::WorkAddressData();

            return orthia::WorkAddressData(
                buffer.data(),
                size,
                nullptr,
                orthia::WorkAddressData::flags_FullValid,
                [buffer = std::move(buffer)](orthia::WorkAddressData*) mutable {}
            );
        }

        int QueryModules(std::vector<orthia::ModuleInfo>& modules,
            int& processModuleOffset,
            std::function<void(const orthia::ModuleInfo&, ModuleDisasmContext&)> contextCallback) override
        {
            processModuleOffset = 0;
            modules.clear();

            auto entries = ParseMaps(m_pid);

            // Collect unique modules: group by path, base = first entry with offset==0
            struct ModCandidate
            {
                unsigned long long baseAddr = 0;
                unsigned long long size     = 0;
                std::string        path;
            };

            std::vector<ModCandidate> candidates;
            {
                std::map<std::string, unsigned long long> seenPath;
                for (auto& e : entries)
                {
                    if (e.path.empty() || !e.isExec)
                        continue;
                    if (e.offset != 0)
                        continue; // only the first segment marks the base

                    if (seenPath.count(e.path))
                        continue;
                    seenPath[e.path] = e.startAddr;

                    ModCandidate c;
                    c.baseAddr = e.startAddr;
                    c.path     = e.path;
                    candidates.push_back(c);

                    ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "Candidate: ", orthia::CLogParamEx(c.baseAddr, 16), " ", c.path);
                }
                // Compute size of each candidate = end of last segment with same path
                for (auto& c : candidates)
                {
                    for (auto& e : entries)
                    {
                        if (e.path == c.path && e.endAddr > c.baseAddr + c.size)
                            c.size = e.endAddr - c.baseAddr;
                    }
                }
            }

            orthia::ProcessReaderAdapter memReader(this);
            orthia::CMemoryStorageOfModifiedData writeCache(&memReader, 0x1000);
            writeCache.SetCacheReads(true);

            std::vector<char> page(0x4000);

            for (auto& c : candidates)
            {
                if (!c.size) c.size = 0x1000;

                orthia::ModuleInfo info;
                // extract short name from path
                {
                    auto pos = c.path.rfind('/');
                    auto shortName = (pos == std::string::npos) ? c.path : c.path.substr(pos + 1);
                    info.name     = orthia::Utf8ToPlatformString(shortName);
                    info.fullName = orthia::Utf8ToPlatformString(c.path);
                }
                info.address = c.baseAddr;
                info.size    = c.size;
                info.lastValidAddress = c.baseAddr + c.size - 1;

                orthia::CMemoryCache moduleCache(&writeCache, c.baseAddr);
                orthia::DianaMemoryStream stream(0, &moduleCache, c.size);

                DianaExecutable exe = {};
                exe.type = DIANA_EXECUTABLE_TYPE_ELF;
                if (!DianaElfFile_Init(&exe.u.elfFile,
                    &stream.parent,
                    c.size,
                    DIANA_ELF_FILE_FLAGS_MODULE_MODE))
                {
                    diana::Guard<diana::ElfFile> elfGuard(&exe.u.elfFile);

                    exe.dianaMode = exe.u.elfFile.dianaMode;
                    info.dianaMode = exe.dianaMode;

                    exe.entryPoint = 0;
                    info.entryPoint = c.baseAddr; // approximate

                    if (contextCallback)
                    {
                        ModuleDisasmContext context;
                        context.executable = &exe;
                        context.stream     = &stream;
                        contextCallback(info, context);
                    }
                }
                modules.push_back(info);
            }
            return 0;
        }

        std::tuple<int, std::shared_ptr<IThreadEnumerator>> CreateThreadEnumerator() override
        {
            auto enumerator = std::make_shared<CPosixThreadEnumerator>(m_pid);
            std::shared_ptr<IThreadEnumerator> result = enumerator;
            return { 0, result };
        }
        
        int GetDianaMode() const override
        {
            if (m_is32bit) 
            {
                return DIANA_MODE32;
            }
            else
            {
                return DIANA_MODE64;
            }
        }
    };

    // -------------------------------------------------------------------------
    // CProcessSystemImpl (POSIX)
    // -------------------------------------------------------------------------

    class CProcessSystemImpl : public IProcessSystem,
                               public std::enable_shared_from_this<CProcessSystemImpl>
    {
    public:
        std::tuple<int, std::shared_ptr<IProcess>> SyncOpenProcess(const oui::ProcessUnifiedId& procId) override
        {
            pid_t pid = (pid_t)procId.pid;

            // Verify the process exists
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d", (int)pid);
            struct stat st;
            if (stat(path, &st) != 0)
                return { errno, nullptr };

            int pointerSize = DetectPointerSize(pid);
            bool is32bit    = (pointerSize == 4);

            std::string commName = ReadProcessComm(pid);
            if (commName.empty())
                commName = std::to_string((unsigned long long)pid);

            String name;
            {
                oui::String::StringStream_type ss;
                ss << OUI_TCSTR("[") << (unsigned long long)pid << OUI_TCSTR("] ")
                   << orthia::Utf8ToPlatformString(commName).c_str();
                name.native = ss.str();
            }

            auto proc = std::make_shared<CPosixProcess>(pid, name, is32bit);
            return { 0, proc };
        }

        void AsyncStartQueryProcess(ThreadPtr_type targetThread,
            const ProcessUnifiedId& procId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler,
            int flags) override
        {
            if (procId.pid)
            {
                int platformError = 0;
                std::shared_ptr<oui::IProcess> proc;
                std::tie(platformError, proc) = SyncOpenProcess(procId);

                auto operation = std::make_shared<Operation<ProcessRecipientHandler_type>>(
                    targetThread, openHandler);
                operation->ReplyWithRetain(operation, proc, platformError);
                return;
            }

            ScanProcesses(targetThread, procId,
                std::move(openHandler), std::move(filterHandler), flags);
        }

    private:
        void ScanProcesses(ThreadPtr_type targetThread,
            const ProcessUnifiedId& procId,
            ProcessRecipientHandler_type&& openHandler,
            OperationPtr_type<QueryProcessHandler_type>&& filterHandler,
            int flags)
        {
            std::vector<ProcessInfo> result;
            int error = 0;

            std::string uppercasePart = Uppercase_Silent(procId.namePart.native);

            DIR* dir = opendir("/proc");
            if (!dir)
            {
                error = errno;
                filterHandler->Reply(filterHandler, procId, result, error);
                return;
            }

            int defaultPointerSize = (sizeof(void*) == 8) ? 8 : 4;

            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr)
            {
                if (!IsDigitString(entry->d_name))
                    continue;

                pid_t pid = (pid_t)atoi(entry->d_name);
                std::string commName = ReadProcessComm(pid);
                if (commName.empty())
                    continue;

                std::string shortName = std::to_string((unsigned long long)pid) + " " + commName;
                std::string upperShort = Uppercase_Silent(shortName);

                if (!uppercasePart.empty())
                {
                    if (upperShort.find(uppercasePart) == std::string::npos)
                        continue;
                }

                ProcessInfo info;
                info.pid         = (unsigned long long)pid;
                info.processName.native = orthia::Utf8ToPlatformString(shortName);
                info.pointerSize = defaultPointerSize;

                if (flags & IProcessSystem::queryFlags_TryOpenProcessAsReader)
                {
                    // Check we can open process memory
                    char memPath[64];
                    snprintf(memPath, sizeof(memPath), "/proc/%d/mem", (int)pid);
                    int fd = open(memPath, O_RDONLY);
                    if (fd >= 0)
                    {
                        close(fd);
                        info.flags |= ProcessInfo::flag_hasReaderAccess;
                    }
                }

                info.pointerSize = DetectPointerSize(pid);

                result.push_back(std::move(info));
            }
            closedir(dir);

            filterHandler->Reply(filterHandler, procId, result, error);
        }
    };

    // -------------------------------------------------------------------------
    // Factory
    // -------------------------------------------------------------------------

    std::shared_ptr<IProcessSystem> CreateDefaultProcessProvider()
    {
        return std::make_shared<CProcessSystemImpl>();
    }

} // namespace oui
