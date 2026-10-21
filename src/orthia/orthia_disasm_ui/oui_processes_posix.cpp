#include "oui_processes.h"
#include "oui_window_thread.h"
#include "oui_base_posix.h"
#include "orthia_process_adapter.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_log.h"
#include "orthia_utils.h"
#include "formats/orthia_image_format.h"
#include "orthia_diana_arch.h"

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

    // 0 when the memory of the process can be read, errno-style error otherwise.
    // The same ptrace-attach check as the reads themselves: opening /proc/<pid>/mem
    // also needs DAC access, which root has no more once orthia drops CAP_DAC_OVERRIDE.
    static int CheckReadAccess(int pid)
    {
        // the kernel checks access before touching the page: EFAULT means allowed
        char byte = 0;
        struct iovec local  = { &byte, 1 };
        struct iovec remote = { nullptr, 1 };
        if (process_vm_readv((pid_t)pid, &local, 1, &remote, 1, 0) >= 0 || errno == EFAULT)
            return 0;
        return errno;
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

    struct ProcessArch
    {
        int dianaMode = DIANA_MODE64;
        int dianaArch = DIANA_ARCH_X86;
    };

    // what orthia itself is built for
    static ProcessArch GetHostArch()
    {
        ProcessArch arch;
#if defined(__aarch64__)
        arch.dianaArch = DIANA_ARCH_AARCH64;
#elif defined(__arm__)
        arch.dianaMode = DIANA_MODE32;
        arch.dianaArch = DIANA_ARCH_ARM32;
#elif defined(__i386__)
        arch.dianaMode = DIANA_MODE32;
#endif
        return arch;
    }

    // The ELF header of /proc/<pid>/exe: the class gives the pointer size, e_machine the architecture.
    // A process whose image can't be read (no access, a kernel thread) is taken as one like orthia
    static ProcessArch DetectProcessArch(pid_t pid)
    {
        ProcessArch arch = GetHostArch();

        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/exe", (int)pid);

        int fd = open(path, O_RDONLY);
        if (fd < 0) return arch;

        unsigned char header[20] = {};
        ssize_t r = read(fd, header, sizeof(header));
        close(fd);

        if (r > 0)
        {
            orthia::FindDianaArchOfElfHeader(header, (size_t)r, &arch.dianaMode, &arch.dianaArch);
        }
        return arch;
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

    // "[pid] comm"
    static String FormatProcessName(pid_t pid)
    {
        std::string commName = ReadProcessComm(pid);
        if (commName.empty())
            commName = std::to_string((unsigned long long)pid);

        String name;
        oui::String::StringStream_type ss;
        ss << OUI_TCSTR("[") << (unsigned long long)pid << OUI_TCSTR("] ")
           << orthia::Utf8ToPlatformString(commName).c_str();
        name.native = ss.str();
        return name;
    }

    // field 22 (starttime) of /proc/<pid>/stat or /proc/<pid>/task/<tid>/stat: clock ticks since boot
    static bool ReadStartTicks(const char* statPath, unsigned long long& ticks)
    {
        std::ifstream f(statPath);
        std::string line;
        if (!f.is_open() || !std::getline(f, line))
            return false;
        // the comm field may hold spaces and ')': the fields start after the last ')'
        auto pos = line.rfind(')');
        if (pos == std::string::npos)
            return false;
        std::istringstream ss(line.substr(pos + 1));
        std::string skipped;
        for (int field = 3; field < 22; ++field)
        {
            if (!(ss >> skipped))
                return false;
        }
        return (bool)(ss >> ticks);
    }

    // seconds since 1970 when the system booted, from /proc/stat; 0 when unknown
    static long long ReadBootTime()
    {
        std::ifstream fstat("/proc/stat");
        std::string statLine;
        while (std::getline(fstat, statLine))
        {
            if (statLine.compare(0, 6, "btime ") == 0)
            {
                return std::strtoll(statLine.c_str() + 6, nullptr, 10);
            }
        }
        return 0;
    }

    static long long StartTicksToUnixTime(unsigned long long ticks)
    {
        const long long btime = ReadBootTime();
        if (btime <= 0)
            return 0;
        long long clkTck = sysconf(_SC_CLK_TCK);
        if (clkTck <= 0)
            clkTck = 100;
        return btime + (long long)(ticks / (unsigned long long)clkTck);
    }

    // changes on every boot; the start ticks only mean something together with it
    static std::string ReadBootId()
    {
        std::ifstream f("/proc/sys/kernel/random/boot_id");
        std::string id;
        std::getline(f, id);
        while (!id.empty() && (id.back() == '\n' || id.back() == '\r' || id.back() == ' '))
            id.pop_back();
        return id;
    }

    // needs no ptrace access: /proc/<pid>/stat is readable for every process
    static ProcessStartInfo QueryProcessStartInfo(pid_t pid)
    {
        ProcessStartInfo info;
        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
        unsigned long long ticks = 0;
        if (!ReadStartTicks(path, ticks))
            return info;
        info.unixSeconds = StartTicksToUnixTime(ticks);
        const std::string bootId = ReadBootId();
        if (!bootId.empty())
            info.key = bootId + ":" + std::to_string(ticks);
        return info;
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

            char path[128];
            snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", (int)m_pid, (int)m_tid);
            unsigned long long ticks = 0;
            if (ReadStartTicks(path, ticks))
            {
                if (const long long startUnix = StartTicksToUnixTime(ticks))
                {
                    info.startTime.InitFromUnixTime(startUnix);
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
        int    m_dianaArch;
        unsigned long long m_pos = 0;
        ProcessStartInfo m_startInfo;

    public:
        CPosixProcess(pid_t pid, const String& name, const ProcessArch& arch)
            : m_pid(pid), m_name(name), m_is32bit(arch.dianaMode == DIANA_MODE32), m_dianaArch(arch.dianaArch),
              m_startInfo(QueryProcessStartInfo(pid))
        {}

        ProcessStartInfo GetStartInfo() const override { return m_startInfo; }
        bool IsRunning() const override
        {
            // the pid may run another process by now: the start tells them apart
            const ProcessStartInfo now = QueryProcessStartInfo(m_pid);
            if (m_startInfo.IsKnown())
                return now.key == m_startInfo.key;
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d", (int)m_pid);
            struct stat st;
            return stat(path, &st) == 0;
        }

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

            auto* pBufferStart = buffer.data();
            return orthia::WorkAddressData(
                pBufferStart,
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
                for (auto& e : entries)
                {
                    if (e.path.empty())
                        continue;
                    
                    if (e.offset == 0)
                    {
                        ModCandidate c;
                        c.baseAddr = e.startAddr;
                        c.path     = e.path;
                        candidates.push_back(c);
                        continue;
                    }
                    if (candidates.empty() || candidates.back().path != e.path)
                    {
                        continue;
                    }
                    
                    ModCandidate & last  = candidates.back();
                    auto distance =  e.endAddr - last.baseAddr;
                    if (last.size < distance)
                        last.size = distance;

                   // ORTHIA_DEV_LOG(orthia::LogSeverity::Debug, "Candidate: ", orthia::CLogParamEx(e.offset, 16), ":",orthia::CLogParamEx(e.startAddr, 16), " ", e.path);
                }
            }

            orthia::ProcessReaderAdapter memReader(this);
            orthia::CMemoryStorageOfModifiedData writeCache(&memReader, 0x1000);
            writeCache.SetCacheReads(true);
            const orthia::IImageFormat& elfFormat = *orthia::FindFormatById("elf");

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

                if (auto image = elfFormat.OpenModuleImage(writeCache, c.baseAddr, c.size))
                {
                    const auto range = image->GetRange();
                    info.dianaMode = range.dianaMode;
                    info.dianaArch = range.dianaArch;
                    // a module without an entry point gets its base, as before
                    info.entryPoint = range.entryPoint ? range.entryPoint : info.address;
                    info.builtInFlags |= elfFormat.GetBuiltInFlag();

                    if (contextCallback)
                    {
                        ModuleDisasmContext context;
                        context.image = image.get();
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

        int GetDianaArch() const override
        {
            return m_dianaArch;
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

            if (int error = CheckReadAccess((int)pid))
                return { error, nullptr };

            auto proc = std::make_shared<CPosixProcess>(pid, FormatProcessName(pid), DetectProcessArch(pid));
            return { 0, proc };
        }

        std::tuple<int, String> SyncQueryProcessName(unsigned long long pid) override
        {
            // reading comm needs no ptrace access, unlike the memory
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d", (int)pid);
            struct stat st;
            if (stat(path, &st) != 0)
                return { errno, String() };
            return { 0, FormatProcessName((pid_t)pid) };
        }

        std::tuple<int, ProcessStartInfo> SyncQueryProcessStartInfo(unsigned long long pid) override
        {
            char path[64];
            snprintf(path, sizeof(path), "/proc/%d", (int)pid);
            struct stat st;
            if (stat(path, &st) != 0)
                return { errno, ProcessStartInfo() };
            return { 0, QueryProcessStartInfo((pid_t)pid) };
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
                    if (!CheckReadAccess((int)pid))
                    {
                        info.flags |= ProcessInfo::flag_hasReaderAccess;
                    }
                }

                info.pointerSize = DetectProcessArch(pid).dianaMode;

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
