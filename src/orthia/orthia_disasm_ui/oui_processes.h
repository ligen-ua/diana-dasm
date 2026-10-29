#pragma once

#include "oui_filesystem.h"
#include "orthia_model_interfaces.h"

namespace orthia
{
    struct IModuleImage;
}
namespace oui
{
    struct ModuleDisasmContext
    {
        orthia::IModuleImage* image = nullptr;     // valid during the callback only
    };

    struct ThreadInfo
    {
        unsigned long long tid = 0;
        unsigned long long startAddress = 0;
        orthia::CCommonDateTime startTime;
    };
    struct IThread
    {
        virtual ~IThread() {};
        virtual int GetInfo(ThreadInfo & info) const = 0;
        virtual int CaptureStack(int framesCount, std::vector<orthia::Address_type> & tstack) const = 0;
    };
    struct IThreadEnumerator
    {
        virtual ~IThreadEnumerator() {}
        virtual std::tuple<int, std::shared_ptr<IThread>> GetNextThread() = 0;
    };
    // When a process started: tells a process from a later one that got the same pid
    struct ProcessStartInfo
    {
        // UTC seconds since 1970, for names shown to people; 0 when unknown.
        // Linux computes it from the boot time, which moves when the clock is set
        long long unixSeconds = 0;
        // exact and stable while the process runs: the creation FILETIME on Windows,
        // "<boot_id>:<start ticks>" on Linux; empty when unknown
        std::string key;

        bool IsKnown() const { return !key.empty(); }
    };

    // The OS allocation that holds an address: VirtualQueryEx on Windows, a /proc/<pid>/maps line on Linux
    struct MemoryRegionInfo
    {
        static const int protect_read = 1;
        static const int protect_write = 2;
        static const int protect_exec = 4;

        static const int type_private = 0;   // VirtualAlloc, anonymous mmap, [heap]
        static const int type_mapped = 1;    // a mapped file (not an image)
        static const int type_image = 2;     // SEC_IMAGE (Windows)

        // the whole allocation: on Windows every region with the same AllocationBase
        unsigned long long allocationBase = 0;
        unsigned long long allocationSize = 0;
        // the part around the address with one protection
        unsigned long long regionBase = 0;
        unsigned long long regionSize = 0;
        int protect = 0;            // of the region holding the address, protect_*
        bool committed = false;     // of the region holding the address
        int type = type_private;
        // a mapped file or a pseudo name ("[heap]", "[anon:...]"); may be empty
        std::string mappedName;
    };

    struct IProcess:IFile
    {
        virtual orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) = 0;
        virtual int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) = 0;
        virtual int QueryModules(std::vector<orthia::ModuleInfo>& modules, int& processModuleOffset, 
            std::function<void (const orthia::ModuleInfo& info, ModuleDisasmContext&)> contextCallback) = 0;
    
        virtual std::tuple<int, std::shared_ptr<IThreadEnumerator>> CreateThreadEnumerator() = 0;
        virtual int GetDianaMode() const = 0;
        // DIANA_ARCH_* of the process image: the code outside its modules (heap, JIT) is disassembled as this
        virtual int GetDianaArch() const = 0;
        // read when the process was opened
        virtual ProcessStartInfo GetStartInfo() const = 0;
        // false once the process has exited (and its pid may run another one)
        virtual bool IsRunning() const = 0;
        // 0 and the allocation holding address; an error when nothing is allocated there
        virtual int QueryMemoryRegion(unsigned long long address, MemoryRegionInfo& info) = 0;
        // every region of the address space that is allocated, by address; allocationSize is 0 (not
        // computed). The callback returns false to stop. An error when the map can't be read
        virtual int EnumMemoryRegions(std::function<bool(const MemoryRegionInfo& info)> callback) { (void)callback; return 1; }
    };

    struct ProcessUnifiedId
    {
        unsigned long long pid = 0;
        String namePart;
        ProcessUnifiedId()
        {
        }
        ProcessUnifiedId(const String& namePart_in)
            :
            namePart(namePart_in)
        {
        }
        ProcessUnifiedId(unsigned long long pid_in)
            :
            pid(pid_in)
        {
        }
        bool IsEmpty() const
        {
            return pid == 0 && namePart.native.empty();
        }
    };
    struct ProcessInfo
    {
        static const int flag_hasReaderAccess = 0x01;

        unsigned long long pid = 0;
        String processName;
        int flags = 0;
        int pointerSize = 0;
    };

    using ProcessRecipientHandler_type = std::function<void(std::shared_ptr<IProcess>, int error)>;
    using QueryProcessHandler_type = std::function<void(std::shared_ptr<BaseOperation> operation, 
        const ProcessUnifiedId& processId, 
        const std::vector<ProcessInfo>& data, 
        int error)>;

    struct IProcessSystem
    {
        const static int queryFlags_TryOpenProcessAsReader = 0x0001;

        virtual ~IProcessSystem() {}
        virtual void AsyncStartQueryProcess(ThreadPtr_type targetThread, 
            const ProcessUnifiedId& processId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler,
            int flags) = 0;

        virtual std::tuple<int, std::shared_ptr<IProcess>> SyncOpenProcess(const oui::ProcessUnifiedId& procId) = 0;

        // "[pid] name", the same as IProcess::GetFullFileNameForUI() of an opened process,
        // but needs no memory access: works for processes that can't be opened as reader
        virtual std::tuple<int, String> SyncQueryProcessName(unsigned long long pid) = 0;
        // also without memory access; an unknown start (no access) is not an error
        virtual std::tuple<int, ProcessStartInfo> SyncQueryProcessStartInfo(unsigned long long pid) = 0;
    };

    // default filesystem
    class CProcessSystem:public IProcessSystem
    {
        std::shared_ptr<IProcessSystem> m_fsImpl;
        CThreadPool m_pool;
    public:
        CProcessSystem();

        void AsyncStartQueryProcess(ThreadPtr_type targetThread,
            const ProcessUnifiedId& fileId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler, 
            int flags) override;

        std::tuple<int, std::shared_ptr<IProcess>> SyncOpenProcess(const oui::ProcessUnifiedId& procId) override;
        std::tuple<int, String> SyncQueryProcessName(unsigned long long pid) override;
        std::tuple<int, ProcessStartInfo> SyncQueryProcessStartInfo(unsigned long long pid) override;
    };

    std::shared_ptr<IProcessSystem> CreateDefaultProcessProvider();

    // A line of Linux /proc/<pid>/task/<tid>/syscall: "<nr> <6 args> <sp> <pc>" in a syscall, "-1 <sp> <pc>"
    // blocked elsewhere, "running" on a CPU (no registers then)
    struct ThreadSyscallState
    {
        bool running = false;
        unsigned long long sp = 0;
        unsigned long long pc = 0;
    };
    // false when the line has neither form
    bool ParseThreadSyscallLine(const std::string& line, ThreadSyscallState& state);

    namespace fsui
    {
        using ProcessCompleteHandler_type = std::function<void(std::shared_ptr<BaseOperation>, std::shared_ptr<IProcess>, const OpenResult&)>;
    }
}
