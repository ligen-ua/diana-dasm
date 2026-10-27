#include "oui_processes.h"
#include "oui_window_thread.h"
#include "oui_base_win32.h"
#include <Psapi.h>
#include <tlhelp32.h>
#include <winternl.h>
#include "orthia_utils.h"
#include "orthia_process_adapter.h"
#include "orthia_memory_cache.h"
#include "orthia_streams.h"
#include "orthia_log.h"
#include "formats/orthia_image_format.h"
#include "orthia_plugins_win32.h"
#include "orthia_exec.h"
#include "orthia_diana_arch.h"

extern "C"
{
#include "diana_processor/diana_processor_context.h"
#include "diana_processor/diana_processor_win32_context.h"
}

namespace oui
{
    const DWORD g_ProcReaderDesiredAccess = PROCESS_QUERY_INFORMATION | STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | PROCESS_VM_OPERATION | PROCESS_VM_READ;

    struct RegionInfo
    {
        ULONGLONG baseAddress, regionSize;
    };

    // the creation time: needs PROCESS_QUERY_LIMITED_INFORMATION
    static ProcessStartInfo QueryProcessStartInfo(HANDLE hProcess)
    {
        ProcessStartInfo info;
        FILETIME creationTime = { 0, };
        FILETIME exitTime = { 0, };
        FILETIME kernelTime = { 0, };
        FILETIME userTime = { 0, };
        if (!GetProcessTimes(hProcess, &creationTime, &exitTime, &kernelTime, &userTime))
        {
            return info;
        }
        ULARGE_INTEGER value;
        value.LowPart = creationTime.dwLowDateTime;
        value.HighPart = creationTime.dwHighDateTime;
        if (!value.QuadPart)
        {
            return info;
        }
        // 100 ns units since 1601
        const ULONGLONG unixEpoch = 116444736000000000ULL;
        if (value.QuadPart > unixEpoch)
        {
            info.unixSeconds = (long long)((value.QuadPart - unixEpoch) / 10000000ULL);
        }
        info.key = std::to_string(value.QuadPart);
        return info;
    }

    // every protection ReadProcessMemory can read: RWX and copy-on-write pages too, not guard pages
    static bool IsReadableProtect(DWORD protect)
    {
        if (protect & (PAGE_GUARD | PAGE_NOACCESS))
        {
            return false;
        }
        const DWORD readable = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return (protect & readable) != 0;
    }

    static int QueryValidRegions(HANDLE hProcess,
        ULONGLONG address,
        ULONGLONG size,
        std::vector<RegionInfo>* pRegions)
    {
        pRegions->clear();
        for (SIZE_T offset = address;size; )
        {
            MEMORY_BASIC_INFORMATION memInfo;
            size_t bytes = VirtualQueryEx(hProcess,
                (char*)offset,
                &memInfo,
                sizeof(memInfo));
            if (!bytes)
            {
                if (offset == address)
                {
                    return GetLastError();
                }
                break;
            }
            if (!memInfo.RegionSize)
            {
                return 1;
            }
            offset = (ULONGLONG)memInfo.BaseAddress + (ULONGLONG)memInfo.RegionSize;

            if ((ULONGLONG)memInfo.BaseAddress < address)
            {
                ULONGLONG diff = address - (ULONGLONG)memInfo.BaseAddress;
                memInfo.RegionSize -= diff;
                memInfo.BaseAddress = (PVOID)address;
            }

            if (memInfo.State == MEM_COMMIT && IsReadableProtect(memInfo.Protect))
            {
                pRegions->push_back({ (ULONGLONG)memInfo.BaseAddress, (ULONGLONG)memInfo.RegionSize});
            }
            if (size > (ULONGLONG)memInfo.RegionSize)
            {
                size -= (ULONGLONG)memInfo.RegionSize;
            }
            else
            {
                break;
            }
        }
        return 0;
    }

    static int ToRegionProtect(DWORD protect)
    {
        if (protect & (PAGE_GUARD | PAGE_NOACCESS))
        {
            return 0;
        }
        switch (protect & 0xFF)
        {
        case PAGE_READONLY:          return MemoryRegionInfo::protect_read;
        case PAGE_READWRITE:
        case PAGE_WRITECOPY:         return MemoryRegionInfo::protect_read | MemoryRegionInfo::protect_write;
        case PAGE_EXECUTE:           return MemoryRegionInfo::protect_exec;
        case PAGE_EXECUTE_READ:      return MemoryRegionInfo::protect_read | MemoryRegionInfo::protect_exec;
        case PAGE_EXECUTE_READWRITE:
        case PAGE_EXECUTE_WRITECOPY: return MemoryRegionInfo::protect_read | MemoryRegionInfo::protect_write | MemoryRegionInfo::protect_exec;
        default:                     return 0;
        }
    }

    static int QueryMemoryRegionWin32(HANDLE hProcess, unsigned long long address, MemoryRegionInfo& info)
    {
        info = MemoryRegionInfo();
        MEMORY_BASIC_INFORMATION memInfo;
        if (!VirtualQueryEx(hProcess, (LPCVOID)(ULONG_PTR)address, &memInfo, sizeof(memInfo)))
        {
            return GetLastError();
        }
        if (memInfo.State == MEM_FREE || !memInfo.AllocationBase)
        {
            return ERROR_INVALID_ADDRESS;
        }
        info.regionBase = (ULONG_PTR)memInfo.BaseAddress;
        info.regionSize = memInfo.RegionSize;
        info.committed = memInfo.State == MEM_COMMIT;
        info.protect = info.committed ? ToRegionProtect(memInfo.Protect) : 0;
        info.type = memInfo.Type == MEM_IMAGE ? MemoryRegionInfo::type_image :
                    memInfo.Type == MEM_MAPPED ? MemoryRegionInfo::type_mapped :
                                                 MemoryRegionInfo::type_private;
        // the allocation: every region from AllocationBase on with the same AllocationBase
        const ULONG_PTR allocationBase = (ULONG_PTR)memInfo.AllocationBase;
        ULONG_PTR end = allocationBase;
        for (;;)
        {
            MEMORY_BASIC_INFORMATION part;
            if (!VirtualQueryEx(hProcess, (LPCVOID)end, &part, sizeof(part)) ||
                (ULONG_PTR)part.AllocationBase != allocationBase || !part.RegionSize)
            {
                break;
            }
            const ULONG_PTR next = (ULONG_PTR)part.BaseAddress + part.RegionSize;
            if (next <= end)
            {
                break;
            }
            end = next;
        }
        info.allocationBase = allocationBase;
        info.allocationSize = end - allocationBase;
        if (info.type != MemoryRegionInfo::type_private)
        {
            std::vector<wchar_t> buffer(4096);
            if (GetMappedFileNameW(hProcess, (LPVOID)(ULONG_PTR)address, buffer.data(), (DWORD)buffer.size() - 1))
            {
                info.mappedName = orthia::PlatformStringToUtf8(buffer.data());
            }
        }
        return 0;
    }

    struct ProcessNoAccessRegion
    {
        size_t relOffset = 0;
        size_t size = 0;
    };

    static void AddRange(std::vector<ProcessNoAccessRegion>& noAccessRegions, LPVOID lpBuffer, char* pOutBuffer, size_t holeSize)
    {
        ProcessNoAccessRegion region;
        region.relOffset = pOutBuffer - (char*)lpBuffer;
        region.size = holeSize;
        noAccessRegions.push_back(region);
    }

    static int SafeReadProcess(std::vector<ProcessNoAccessRegion>& noAccessRegions,
        HANDLE hProcess,
        ULONGLONG offset,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead)
    {
        noAccessRegions.clear();

        // check optimistic path, likely
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(hProcess, (LPCVOID)offset, lpBuffer, nNumberOfBytesToRead, &bytesRead) && 
            nNumberOfBytesToRead == bytesRead)
        {
            return 0;
        }
        // damn bad, it has holes
        std::vector<RegionInfo> regions;
        int error = QueryValidRegions(hProcess, offset, nNumberOfBytesToRead, &regions);
        if (error)
        {
            return error;
        }
        if (regions.empty())
        {
            // no access
            memset(lpBuffer, 0, nNumberOfBytesToRead);
            AddRange(noAccessRegions, lpBuffer, (char*)lpBuffer, nNumberOfBytesToRead);
            return 0;
        }


        char* pOutBuffer = (char*)lpBuffer;
        ULONGLONG outAddress = offset;
        ULONGLONG sizeToGo = nNumberOfBytesToRead;

        bool fail = false;
        for (const auto& reg : regions)
        {
            if (!sizeToGo)
            {
                break;
            }
            if (reg.baseAddress > outAddress)
            {
                // we got a hole
                auto holeSize = std::min(reg.regionSize, reg.baseAddress - outAddress);
                holeSize = std::min(sizeToGo, holeSize);
                
                memset(pOutBuffer, 0, (size_t)holeSize);
                AddRange(noAccessRegions, lpBuffer, pOutBuffer, holeSize);

                pOutBuffer += holeSize;
                outAddress += holeSize;
                sizeToGo -= holeSize;
            }
            // So here reg.baseAddress <= outAddress
            // check API result for sanity
            auto lastByteAddress = reg.baseAddress + (reg.regionSize - 1);
            if (lastByteAddress < outAddress)
            {
                // region is far away, it should never happened but it has
                continue;
            }

            // means outAddress is valid, detect the region's size
            auto dataSize = (lastByteAddress - outAddress) + 1;
            auto sizeToRead = std::min(sizeToGo, dataSize);

            bytesRead = 0;
            if (!ReadProcessMemory(hProcess, (LPCVOID)outAddress, pOutBuffer, (SIZE_T)sizeToRead, &bytesRead) ||
                (SIZE_T)sizeToRead != bytesRead)
            {
                fail = true;
                break;
            }
            // we got a range
            pOutBuffer += sizeToRead;
            outAddress += sizeToRead;
            sizeToGo -= sizeToRead;
        }

        if (sizeToGo)
        {
            memset(pOutBuffer, 0, (size_t)sizeToGo);
            AddRange(noAccessRegions, lpBuffer, pOutBuffer, sizeToGo);
        }
        if (fail)
        {
            // no access
            memset(lpBuffer, 0, nNumberOfBytesToRead);
            AddRange(noAccessRegions, lpBuffer, (char*)lpBuffer, nNumberOfBytesToRead);
            return 0;
        }
        return 0;
    }

    bool CheckIsWow64Process(HANDLE hProcess)
    {
        typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
        static LPFN_ISWOW64PROCESS fnIsWow64Process = 0;
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            fnIsWow64Process = (LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
        });

        if (fnIsWow64Process)
        {
            BOOL wow64 = FALSE;
            if (fnIsWow64Process(hProcess, &wow64))
            {
                return wow64;
            }
        }
        return false;
    }
    bool CheckIsWindows64()
    {
#if _WIN64
        return true;
#else 
        // I'm a 32 bit, soo
        return CheckIsWow64Process(GetCurrentProcess());
#endif
    }

    bool CheckIs32BitProcess(HANDLE hProcess)
    {
        if (!CheckIsWindows64())
        {
            return true;
        }
        return CheckIsWow64Process(hProcess);
    }

    struct ProcessArch
    {
        int dianaMode = DIANA_MODE64;
        int dianaArch = DIANA_ARCH_X86;
    };

    // PROCESS_MACHINE_INFORMATION, ProcessMachineTypeInfo = 9 (Windows 11): not in the older SDKs
    struct ProcessMachineInformation
    {
        USHORT processMachine;
        USHORT reserved;
        ULONG machineAttributes;
    };
    const int g_processMachineTypeInfo = 9;

    // The machine of the process from what this Windows can tell, the rules are in SelectWin32ProcessMachine.
    // Both functions are looked up: IsWow64Process2 is Windows 10 1709+, GetProcessInformation Windows 8+,
    // and Windows without them runs x86 and x64 only
    ProcessArch DetectProcessArch(HANDLE hProcess)
    {
        typedef BOOL(WINAPI* IsWow64Process2_type)(HANDLE, USHORT*, USHORT*);
        typedef BOOL(WINAPI* GetProcessInformation_type)(HANDLE, int, LPVOID, DWORD);
        static IsWow64Process2_type fnIsWow64Process2 = 0;
        static GetProcessInformation_type fnGetProcessInformation = 0;
        static std::once_flag flag;
        std::call_once(flag, [&]() {
            HMODULE kernel32 = GetModuleHandle(TEXT("kernel32"));
            fnIsWow64Process2 = (IsWow64Process2_type)GetProcAddress(kernel32, "IsWow64Process2");
            fnGetProcessInformation = (GetProcessInformation_type)GetProcAddress(kernel32, "GetProcessInformation");
        });

        int processMachineInfo = 0;
        if (fnGetProcessInformation)
        {
            ProcessMachineInformation info = { 0, };
            if (fnGetProcessInformation(hProcess, g_processMachineTypeInfo, &info, sizeof(info)))
            {
                processMachineInfo = info.processMachine;
            }
        }
        int wow64Machine = 0;
        int nativeMachine = 0;
        if (fnIsWow64Process2)
        {
            USHORT processMachine = 0;
            USHORT hostMachine = 0;
            if (fnIsWow64Process2(hProcess, &processMachine, &hostMachine))
            {
                wow64Machine = processMachine;
                nativeMachine = hostMachine;
            }
        }

        ProcessArch arch;
        const int machine = orthia::SelectWin32ProcessMachine(processMachineInfo, wow64Machine, nativeMachine);
        if (machine && orthia::FindDianaArchOfPeMachine(machine, &arch.dianaMode, &arch.dianaArch))
        {
            return arch;
        }
        arch.dianaMode = CheckIs32BitProcess(hProcess) ? DIANA_MODE32 : DIANA_MODE64;
        arch.dianaArch = DIANA_ARCH_X86;
        return arch;
    }


    int LoadModulesFromProcess(HANDLE hProc, std::vector<HMODULE>& modules)
    {
        const int initialModulesCount = 1024;
        if (modules.empty())
        {
            modules.resize(initialModulesCount);
        }
        for (;;)
        {
            DWORD sizeNeeded = 0;
            BOOL res = EnumProcessModules(hProc,
                &modules[0],
                (DWORD)(modules.size() * sizeof(HMODULE)),
                &sizeNeeded);
            if (!res)
            {
                return GetLastError();
            }
            DWORD maxCount = sizeNeeded / sizeof(HMODULE);
            if (maxCount < modules.size())
            {
                modules.resize(maxCount);
                break;
            }
            modules.resize(maxCount * 2);
        }
        return 0;
    }
    class CWin32Thread;
    class CProcess;
    

    struct NtFunctions
    {
        decltype(NtQueryInformationThread)* ntQueryInformationThread;
    };

    struct IThreadOpener
    {
        virtual ~IThreadOpener() {}
        virtual std::tuple<int, std::shared_ptr<CWin32Thread>> OpenNextThread(HANDLE hProcess, HANDLE hThread, std::shared_ptr<CProcess> process) = 0;
        virtual NtFunctions GetNtFunctions() = 0;
    };
    class CWin32Thread:public IThread
    {
        mutable orthia::CHandleGuard m_handleGuard;
        mutable std::weak_ptr<IProcess> m_proc;
        std::shared_ptr<IThreadOpener> m_procSystem;
    public:
        CWin32Thread(orthia::CHandleGuard & handleGuard, std::shared_ptr<IProcess> proc, std::shared_ptr<IThreadOpener> procSystem)
            :
                m_proc(proc),
                m_procSystem(procSystem)
        {
            m_handleGuard.Reset(handleGuard.Release());
        }
        int GetInfo(ThreadInfo& info) const override
        {
            auto proc = m_proc.lock();
            if (!proc)
            {
                return ERROR_INVALID_FUNCTION;
            }
            orthia::Address_type startAddress = 0;
            auto ntStatus = m_procSystem->GetNtFunctions().ntQueryInformationThread(
                m_handleGuard.Get(),
                (THREADINFOCLASS)0x9,//ThreadQuerySetWin32StartAddress,
                &startAddress,
                sizeof(startAddress),
                NULL
            );
            info.tid = GetThreadId(m_handleGuard.Get());
            info.startAddress = startAddress;

            FILETIME creationTime = {0, };
            FILETIME exitTime = { 0, };
            FILETIME kernelTime = { 0, };
            FILETIME userTime = { 0, };

            if (GetThreadTimes(m_handleGuard.Get(),
                &creationTime,
                &exitTime,
                &kernelTime,
                &userTime))
            {
                SYSTEMTIME st;
                FileTimeToSystemTime(&creationTime, &st);
                info.startTime.InitFromSystemTime(st);
            }
            return 0;
        }
        int CaptureStack(int framesCount, std::vector<orthia::Address_type>& tstack) const
        {
            auto proc = m_proc.lock();
            if (!proc)
            {
                return ERROR_INVALID_FUNCTION;
            }
            // the stack is walked by the x86 emulator from the thread's registers
            if (proc->GetDianaArch() != DIANA_ARCH_X86)
            {
                return ERROR_NOT_SUPPORTED;
            }
            Diana_Processor_Registers_Context dianaContext;
            const int mode = proc->GetDianaMode();
            if (mode == DIANA_MODE32)
            {
#if defined(_M_IX86)
                // a 32-bit orthia sees x86 threads in their own context, under WOW64 too
                CONTEXT context;
                context.ContextFlags = CONTEXT_FULL;
                if (!GetThreadContext(m_handleGuard.Get(), &context))
                {
                    return GetLastError();
                }
                DI_CHECK(DianaProcessor_ConvertContextToIndependent_Win32((DIANA_CONTEXT_NTLIKE_32*)&context, &dianaContext));
#else
                // x64 and ARM64 Windows run x86 processes in WOW64
                WOW64_CONTEXT wowContext;
                wowContext.ContextFlags = CONTEXT_FULL;
                if (!Wow64GetThreadContext(m_handleGuard.Get(), &wowContext))
                {
                    return GetLastError();
                }
                DI_CHECK(DianaProcessor_ConvertContextToIndependent_Win32((DIANA_CONTEXT_NTLIKE_32*) &wowContext, &dianaContext));
#endif
            }
            else
            {
#if defined(_M_X64)
                CONTEXT context;
                context.ContextFlags = CONTEXT_FULL;
                if (!GetThreadContext(m_handleGuard.Get(), &context))
                {
                    return GetLastError();
                }
                DI_CHECK(DianaProcessor_ConvertContextToIndependent_X64((DIANA_CONTEXT_NTLIKE_64*)&context, &dianaContext));
#else
                // a 32-bit orthia can't read 64-bit registers, and ARM64 Windows gives an emulated x64
                // thread's registers in the ARM64 layout
                return ERROR_NOT_SUPPORTED;
#endif
            }
            orthia::ProcessReaderAdapter memReader(proc.get());
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            orthia::CMemoryStorageOfModifiedData writeCache(&memReader, sysInfo.dwPageSize);
            writeCache.SetCacheReads(true);

            orthia::CProcessor processor(&writeCache, mode, 0);
            processor.Init();
            DI_CHECK(DianaProcessor_InitContext(processor.GetSelf(), &dianaContext));

            processor.QueryCallStack(&tstack, framesCount);
            return 0;
        }
        HANDLE GetHandle()
        {
            return m_handleGuard.Get();
        }
    };
    class CProcess;
    class CWin32ThreadEnumerator :public IThreadEnumerator, Noncopyable
    {
        std::shared_ptr<CProcess> m_process;
        std::shared_ptr<IThreadOpener> m_opener;
        std::shared_ptr<CWin32Thread> m_thread;
    public:
        CWin32ThreadEnumerator(std::shared_ptr<IThreadOpener> opener, std::shared_ptr<CProcess> process)
            :
            m_opener(opener), m_process(process)
        {
        }
        std::tuple<int, std::shared_ptr<IThread>> GetNextThread() override;
    };

    class CProcess :public IProcess, Noncopyable, public std::enable_shared_from_this<CProcess>
    {
        String m_name;
        HANDLE m_hProc;
        bool m_is32bit;
        int m_dianaArch;
        LARGE_INTEGER m_distance;
        ULONG m_processID;
        std::weak_ptr<IThreadOpener> m_opener;
        ProcessStartInfo m_startInfo;
    public:
        CProcess(const String& name, HANDLE hProc, const ProcessArch& arch, ULONG processID, std::shared_ptr<IThreadOpener> opener)
            :
            m_name(name),
            m_hProc(hProc),
            m_is32bit(arch.dianaMode == DIANA_MODE32),
            m_dianaArch(arch.dianaArch),
            m_processID(processID),
            m_opener(opener),
            m_startInfo(QueryProcessStartInfo(hProc))
        {
            m_distance.QuadPart = 0;
        }
        ProcessStartInfo GetStartInfo() const override
        {
            return m_startInfo;
        }
        bool IsRunning() const override
        {
            // the handle keeps the pid from being reused; SYNCHRONIZE is in g_ProcReaderDesiredAccess
            return m_hProc && m_hProc != INVALID_HANDLE_VALUE && WaitForSingleObject(m_hProc, 0) == WAIT_TIMEOUT;
        }
        int QueryMemoryRegion(unsigned long long address, MemoryRegionInfo& info) override
        {
            return QueryMemoryRegionWin32(m_hProc, address, info);
        }
        int EnumMemoryRegions(std::function<bool(const MemoryRegionInfo& info)> callback) override
        {
            ULONG_PTR address = 0;
            for (;;)
            {
                MEMORY_BASIC_INFORMATION memInfo;
                if (!VirtualQueryEx(m_hProc, (LPCVOID)address, &memInfo, sizeof(memInfo)))
                {
                    // past the last region
                    const DWORD error = GetLastError();
                    return error == ERROR_INVALID_PARAMETER ? 0 : (int)error;
                }
                if (memInfo.State != MEM_FREE && memInfo.AllocationBase)
                {
                    MemoryRegionInfo info;
                    info.allocationBase = (ULONG_PTR)memInfo.AllocationBase;
                    info.regionBase = (ULONG_PTR)memInfo.BaseAddress;
                    info.regionSize = memInfo.RegionSize;
                    info.committed = memInfo.State == MEM_COMMIT;
                    // PAGE_GUARD and PAGE_NOACCESS give 0: never read
                    info.protect = info.committed ? ToRegionProtect(memInfo.Protect) : 0;
                    info.type = memInfo.Type == MEM_IMAGE ? MemoryRegionInfo::type_image :
                                memInfo.Type == MEM_MAPPED ? MemoryRegionInfo::type_mapped :
                                                             MemoryRegionInfo::type_private;
                    if (info.type != MemoryRegionInfo::type_private)
                    {
                        std::vector<wchar_t> buffer(4096);
                        if (GetMappedFileNameW(m_hProc, memInfo.BaseAddress, buffer.data(), (DWORD)buffer.size() - 1))
                        {
                            info.mappedName = orthia::PlatformStringToUtf8(buffer.data());
                        }
                    }
                    if (!callback(info))
                    {
                        return 0;
                    }
                }
                const ULONG_PTR next = (ULONG_PTR)memInfo.BaseAddress + memInfo.RegionSize;
                if (next <= address)
                {
                    return 0;
                }
                address = next;
            }
        }
        ~CProcess()
        {
            Reset(String(), 0);
        }
        HANDLE GetProcessHandle()
        {
            return m_hProc;
        }
        void Reset(const String& name, HANDLE hProc)
        {
            if (hProc == m_hProc)
            {
                return;
            }
            if (m_hProc != 0 && m_hProc != INVALID_HANDLE_VALUE)
            {
                CloseHandle(m_hProc);
            }
            m_hProc = hProc;
            m_name = name;
        }
        std::tuple<int, unsigned long long> GetSizeInBytes() const override
        {
            LARGE_INTEGER size;
            if (m_is32bit)
            {
                size.QuadPart = MAXUINT32;
            }
            else
            {
                size.QuadPart = MAXUINT64;
            }
            return std::make_tuple(0, size.QuadPart);
        }

        int MoveToBegin(unsigned long long offset)
        {
            m_distance.QuadPart = offset;
            return 0;
        }
        oui::String GetFullFileName() const override
        {
            return m_name;
        }
        oui::String GetFullFileNameForUI() const override
        {
            return m_name;
        }
        int QueryModules(std::vector<orthia::ModuleInfo>& modules, int& processModuleOffset, std::function<void(const orthia::ModuleInfo & info, ModuleDisasmContext&)> contextCallback) override
        {
            processModuleOffset = 0;
            modules.clear();

            std::vector<HMODULE> modulesVec;
            int errorCode = LoadModulesFromProcess(m_hProc, modulesVec);
            if (errorCode)
            {
                return errorCode;
            }


            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            orthia::ProcessReaderAdapter memReader(this);
            orthia::CMemoryStorageOfModifiedData writeCache(&memReader, sysInfo.dwPageSize);
            writeCache.SetCacheReads(true);
            const orthia::IImageFormat& peFormat = *orthia::FindFormatById("pe");

            std::vector<wchar_t> buffer(4096);
            for (auto hmod : modulesVec)
            {
                orthia::ModuleInfo info;
                if (GetModuleFileNameEx(m_hProc,
                    hmod,
                    buffer.data(),
                    (DWORD)buffer.size() - 1))
                {
                    info.fullName = buffer.data();
                    orthia::UnparseFileNameFromFullFileName(info.fullName, &info.name);
                }
                else
                {
                    info.name = orthia::ToWideStringAsHex((OPERAND_SIZE)hmod);
                    info.fullName = L"<unknown>";
                }
                info.address = (orthia::Address_type)hmod;

                MODULEINFO lowLevelInfo = { 0, };
                if (GetModuleInformation(m_hProc,
                    hmod,
                    &lowLevelInfo,
                    sizeof(lowLevelInfo)))
                {
                    info.size = lowLevelInfo.SizeOfImage;
                }
                else
                {
                    info.size = 0x10000000;
                }
                if (!info.size)
                {
                    continue;
                }

                // prepare last valid
                info.lastValidAddress = info.address;
                Diana_SafeAdd(&info.lastValidAddress, info.size - 1);


                if (auto image = peFormat.OpenModuleImage(writeCache, info.address, info.size))
                {
                    const auto range = image->GetRange();
                    info.dianaMode = range.dianaMode;
                    info.dianaArch = range.dianaArch;
                    // a module without an entry point gets its base, as before
                    info.entryPoint = range.entryPoint ? range.entryPoint : info.address;
                    info.builtInFlags |= peFormat.GetBuiltInFlag() | image->GetExtraBuiltInFlags();

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
        int ReadExactEx2(unsigned long long offset, void * pBuffer, size_t size) override
        {
            std::vector<ProcessNoAccessRegion> noAccessRegions;
            DWORD readBytes = 0;
            return SafeReadProcess(noAccessRegions, m_hProc, offset, pBuffer, (DWORD)size, &readBytes);
        }
        std::tuple<int, std::shared_ptr<IThreadEnumerator>> CreateThreadEnumerator() override
        {
            auto opener = m_opener.lock();
            if (!opener)
            {
                return { ERROR_FILE_NOT_FOUND, nullptr };
            }
            std::shared_ptr<IThreadEnumerator> res(std::make_shared<CWin32ThreadEnumerator>(opener, shared_from_this()));
            return { 0, res };
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

        orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) override
        {
            std::vector<ProcessNoAccessRegion> noAccessRegions;
            DWORD readBytes = 0;
            std::vector<char> buffer(size);
            if (int error = SafeReadProcess(noAccessRegions, m_hProc, offset, buffer.data(), (DWORD)size, &readBytes))
            {
                return orthia::WorkAddressData();
            }
            auto ptr = buffer.data();
            if (noAccessRegions.empty())
            {
                // lucky case
                return orthia::WorkAddressData(
                    ptr,
                    size,
                    nullptr,
                    orthia::WorkAddressData::flags_FullValid,
                    [buffer = std::move(buffer)](orthia::WorkAddressData*) mutable {
                }
                );
            }
            // more complex case
            std::vector<char> flags(size);
            auto* pFlagsStart = flags.data();
            for (auto& reg : noAccessRegions)
            {
                memset(pFlagsStart + reg.relOffset, orthia::WorkAddressData::dataFlags_Invalid, reg.size);
            }
            return orthia::WorkAddressData(
                ptr,
                size,
                pFlagsStart,
                0,
                [
                    buffer = std::move(buffer),
                    flags = std::move(flags)
                ](orthia::WorkAddressData*) mutable {
            }
            );
        }
        int ReadExact(std::shared_ptr<BaseOperation> operation, unsigned long long offset, size_t size, std::vector<char>& peFile) override
        {
            if (offset != (unsigned long long)(-1))
            {
                int err = MoveToBegin(offset);
                if (err)
                {
                    return err;
                }
            }
            peFile.resize(size);

            DWORD pageSize = 16*4096;
            std::vector<char> page(pageSize);

            auto ptr = peFile.data();
            size_t sizeToCopy = size;
            std::vector<ProcessNoAccessRegion> noAccessRegions;
            for (; sizeToCopy; )
            {
                DWORD sizeToRead = pageSize;
                if (sizeToCopy < pageSize)
                {
                    sizeToRead = (DWORD)sizeToCopy;
                }
                DWORD readBytes = 0;
                if (int error = SafeReadProcess(noAccessRegions, m_hProc, m_distance.QuadPart, ptr, sizeToRead, &readBytes))
                {
                    return error;
                }
                m_distance.QuadPart += readBytes;
                ptr += readBytes;
                sizeToCopy -= readBytes;

                if (operation && operation->IsCancelled())
                {
                    return ERROR_CANCELLED;
                }
            }
            return 0;
        }
    };


    std::tuple<int, std::shared_ptr<IThread>> CWin32ThreadEnumerator::GetNextThread()
    {
        HANDLE hProc = m_process->GetProcessHandle();

        HANDLE hThread = 0;
        if (m_thread)
        {
            hThread = m_thread->GetHandle();
        }
        auto [error, thread] = m_opener->OpenNextThread(hProc, hThread, m_process);
        if (error || !thread)
        {
            if (error == ERROR_NO_MORE_ITEMS)
            {
                error = 0;
            }
            return { error, thread };
        }
        m_thread = thread;
        return { 0, m_thread };
    }

    class CProcessSystemImpl :public IProcessSystem, public std::enable_shared_from_this<CProcessSystemImpl>, public IThreadOpener
    {
        orthia::OpenProcess_type m_openProcess = 0;
        orthia::NtGetNextThread_type m_getNextThread = 0;
        orthia::CWin32OpenProcessPlugin m_openProcessPlugin;
        orthia::CDll m_ntdll;
        decltype(RtlNtStatusToDosError)* m_RtlNtStatusToDosError = 0;
        NtFunctions ntFunctions;
    public:
        CProcessSystemImpl()
        {
            m_openProcess = &::OpenProcess;
            m_ntdll.Reset(ORTHIA_TCSTR("ntdll.dll"));
            m_ntdll.QueryFunction("NtGetNextThread", &m_getNextThread, false);
            m_ntdll.QueryFunction("RtlNtStatusToDosError", &m_RtlNtStatusToDosError, false);
            m_ntdll.QueryFunction("NtQueryInformationThread", &ntFunctions.ntQueryInformationThread, false);
        }
        ~CProcessSystemImpl()
        {
        }
        NtFunctions GetNtFunctions()
        {
            return ntFunctions;
        }
        std::tuple<int, std::shared_ptr<CWin32Thread>> OpenNextThread(HANDLE hProcess, HANDLE hThread, std::shared_ptr<CProcess> process) override
        {
            HANDLE hNewThread = 0;
            auto status = m_getNextThread(hProcess, hThread, THREAD_QUERY_INFORMATION|THREAD_GET_CONTEXT, 0, 0, &hNewThread);
            if (!NT_SUCCESS(status))
            {
                status = m_getNextThread(hProcess, hThread, THREAD_QUERY_INFORMATION, 0, 0, &hNewThread);
            }
            if (!NT_SUCCESS(status))
            {
                return { m_RtlNtStatusToDosError(status), nullptr };
            }
            orthia::CHandleGuard guard(hNewThread);
            auto res = std::make_shared<CWin32Thread>(guard, process, shared_from_this());
            return { 0, res };
        }

        void LoadPlugins()
        {
            if (m_openProcess != &::OpenProcess)
            {
                return;
            }
            if (m_openProcessPlugin.Load())
            {
                m_openProcess = m_openProcessPlugin.GetOpenProcess();

                if (auto getNextThread = m_openProcessPlugin.GetGetNextThread())
                {
                    m_getNextThread = getNextThread;
                }
            }
        }
        std::tuple<int, std::shared_ptr<IProcess>> SyncOpenProcess(const oui::ProcessUnifiedId& procId) override
        {
            LoadPlugins();
            int error = 0;
            std::shared_ptr<IProcess> proc;
            if (HANDLE hProc = m_openProcess(g_ProcReaderDesiredAccess, FALSE, (DWORD)procId.pid))
            {
                oui::ScopedGuard handlerGuard([&]() {
                    CloseHandle(hProc);
                });

                const ProcessArch arch = DetectProcessArch(hProc);

                std::vector<wchar_t> buf(4096);
                if (!GetProcessImageFileNameW(hProc, buf.data(), (DWORD)(buf.size() - 1)))
                {
                    error = GetLastError();
                }
                else
                {
                    oui::String shortName;
                    orthia::UnparseFileNameFromFullFileName<oui::String::string_type>(buf.data(), &shortName.native);

                    oui::String::StringStream_type res;
                    res << OUI_TCSTR("[") << procId.pid << OUI_TCSTR("] ") << shortName.native;

                    proc = std::make_shared<CProcess>(res.str(), hProc, arch, (DWORD)procId.pid, shared_from_this());
                    handlerGuard.Release();
                }
            }
            else
            {
                error = GetLastError();
            }
            return std::make_tuple(error, proc);
        }

        std::tuple<int, String> SyncQueryProcessName(unsigned long long pid) override
        {
            HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            if (!hProc)
            {
                int error = GetLastError();
                if (error != ERROR_ACCESS_DENIED)
                {
                    return std::make_tuple(error, String());
                }
                // the open process plugin may still get through
                std::shared_ptr<IProcess> proc;
                std::tie(error, proc) = SyncOpenProcess(oui::ProcessUnifiedId(pid));
                return std::make_tuple(error, proc ? proc->GetFullFileNameForUI() : String());
            }
            oui::ScopedGuard handlerGuard([&]() {
                CloseHandle(hProc);
            });

            std::vector<wchar_t> buf(4096);
            if (!GetProcessImageFileNameW(hProc, buf.data(), (DWORD)(buf.size() - 1)))
            {
                return std::make_tuple((int)GetLastError(), String());
            }
            oui::String shortName;
            orthia::UnparseFileNameFromFullFileName<oui::String::string_type>(buf.data(), &shortName.native);

            oui::String::StringStream_type res;
            res << OUI_TCSTR("[") << pid << OUI_TCSTR("] ") << shortName.native;
            return std::make_tuple(0, String(res.str()));
        }

        std::tuple<int, ProcessStartInfo> SyncQueryProcessStartInfo(unsigned long long pid) override
        {
            bool canWait = true;
            HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, (DWORD)pid);
            if (!hProc && GetLastError() == ERROR_ACCESS_DENIED)
            {
                canWait = false;
                hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
            }
            if (!hProc)
            {
                int error = GetLastError();
                if (error != ERROR_ACCESS_DENIED)
                {
                    return std::make_tuple(error, ProcessStartInfo());
                }
                // the open process plugin may still get through; when it doesn't, the
                // process exists, its start is just unknown
                std::shared_ptr<IProcess> proc;
                std::tie(error, proc) = SyncOpenProcess(oui::ProcessUnifiedId(pid));
                return std::make_tuple(0, proc ? proc->GetStartInfo() : ProcessStartInfo());
            }
            oui::ScopedGuard handlerGuard([&]() {
                CloseHandle(hProc);
            });
            // an exited process stays openable while someone holds a handle to it
            if (canWait && WaitForSingleObject(hProc, 0) == WAIT_OBJECT_0)
            {
                return std::make_tuple((int)ERROR_INVALID_PARAMETER, ProcessStartInfo());
            }
            return std::make_tuple(0, QueryProcessStartInfo(hProc));
        }

        void AsyncStartQueryProcess(ThreadPtr_type targetThread,
            const ProcessUnifiedId& fileId,
            ProcessRecipientHandler_type openHandler,
            OperationPtr_type<QueryProcessHandler_type> filterHandler,
            int flags)
        {
            if (!fileId.pid)
            {
                ScanProcesses(targetThread,
                    fileId,
                    std::move(openHandler),
                    std::move(filterHandler),
                    flags);
                return;
            }

            // here we need to open it

            int platformError = 0;
            std::shared_ptr<oui::IProcess> proc;
            std::tie(platformError, proc) = SyncOpenProcess(oui::ProcessUnifiedId((DWORD)fileId.pid));

            auto operation = std::make_shared<Operation<ProcessRecipientHandler_type>>(
                targetThread,
                openHandler);

            operation->ReplyWithRetain(operation, proc, platformError);
        }
        void ScanProcesses(ThreadPtr_type targetThread,
            const ProcessUnifiedId& fileId,
            ProcessRecipientHandler_type&& openHandler,
            OperationPtr_type<QueryProcessHandler_type>&& filterHandler,
            int flags)
        {
            std::vector<ProcessInfo> result;
            int error = 0;

            std::wstring uppercasePart = Uppercase_Silent(fileId.namePart.native);

            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snapshot == INVALID_HANDLE_VALUE)
            {
                error = GetLastError();
                filterHandler->Reply(filterHandler, fileId, result, error);
                return;
            }

            oui::ScopedGuard guard([=]() {  CloseHandle(snapshot);   });

            PROCESSENTRY32W processEntry;
            processEntry.dwSize = sizeof(PROCESSENTRY32);

            if (!Process32FirstW(snapshot, &processEntry))
            {
                error = GetLastError();
                filterHandler->Reply(filterHandler, fileId, result, error);
                return;
            }

            auto defaultPointerSize = CheckIsWindows64() ? 8 : 4;
            do
            {                
                std::wstring fullName(processEntry.szExeFile);
             
                std::wstring shortName;
                orthia::UnparseFileNameFromFullFileName(fullName, &shortName);

                shortName = std::to_wstring(processEntry.th32ProcessID) + L" " + shortName;
                std::wstring uppercaseShortName = Uppercase_Silent(shortName);

                if (!uppercasePart.empty())
                {
                    auto res = uppercaseShortName.find(uppercasePart.c_str());
                    if (res == std::wstring::npos)
                    {
                        continue;
                    }
                }
                ProcessInfo info;
                info.pid = processEntry.th32ProcessID;
                info.processName = shortName;
                info.pointerSize = defaultPointerSize;

                if (flags & oui::IProcessSystem::queryFlags_TryOpenProcessAsReader)
                {
                    if (HANDLE hProc = ::OpenProcess(g_ProcReaderDesiredAccess, FALSE, (DWORD)info.pid))
                    {
                        CloseHandle(hProc);
                        info.flags |= ProcessInfo::flag_hasReaderAccess;
                    }
                }

                const DWORD desiredAccess = PROCESS_QUERY_LIMITED_INFORMATION;
                if (HANDLE hProc = ::OpenProcess(desiredAccess, FALSE, (DWORD)info.pid))
                {
                    oui::ScopedGuard handlerGuard([&]() {
                        CloseHandle(hProc);
                    });

                    info.pointerSize = DetectProcessArch(hProc).dianaMode;
                }

                result.push_back(std::move(info));
            }
            while (Process32NextW(snapshot, &processEntry));
            filterHandler->Reply(filterHandler, fileId, result, error);
        }    
    };

    std::shared_ptr<IProcessSystem> CreateDefaultProcessProvider()
    {
        return std::make_shared<CProcessSystemImpl>();
    }
}
