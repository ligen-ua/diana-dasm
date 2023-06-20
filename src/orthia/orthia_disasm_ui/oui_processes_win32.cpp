#include "oui_processes.h"
#include "oui_window_thread.h"
#include "oui_base_win32.h"
#include <Psapi.h>
#include <tlhelp32.h>
#include "orthia_utils.h"
namespace oui
{
    struct RegionInfo
    {
        ULONGLONG baseAddress, regionSize;
    };

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
            if (memInfo.State == MEM_COMMIT &&
                memInfo.Type == MEM_PRIVATE &&
                (memInfo.Protect & (PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_READWRITE)) != 0)
            {
                pRegions->push_back({ (ULONGLONG)memInfo.BaseAddress,
                    (ULONGLONG)memInfo.RegionSize});
            }
            offset = (ULONGLONG)memInfo.BaseAddress + (ULONGLONG)memInfo.RegionSize;
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

    static int SafeReadProcess(HANDLE hProcess,
        ULONGLONG offset,
        LPVOID lpBuffer,
        DWORD nNumberOfBytesToRead,
        LPDWORD lpNumberOfBytesRead)
    {
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
            return 0;
        }


        char* pOutBuffer = 0;
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
                pOutBuffer += holeSize;
                outAddress += holeSize;
                sizeToGo -= holeSize;
                continue;
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

        if (fail)
        {
            // no access
            memset(lpBuffer, 0, nNumberOfBytesToRead);
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
    class CProcess :public IProcess, Noncopyable
    {
        String m_name;
        HANDLE m_hProc;
        bool m_is32bit;
        LARGE_INTEGER m_distance;
    public:
        CProcess(const String& name, HANDLE hProc, bool is32bit)
            :
            m_name(name),
            m_hProc(hProc),
            m_is32bit(is32bit)
        {
            m_distance.QuadPart = 0;
        }
        ~CProcess()
        {
            Reset(String(), 0);
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
            for (; sizeToCopy; )
            {
                DWORD sizeToRead = pageSize;
                if (sizeToCopy < pageSize)
                {
                    sizeToRead = (DWORD)sizeToCopy;
                }
                DWORD readBytes = 0;
                if (int error = SafeReadProcess(m_hProc, m_distance.QuadPart, ptr, sizeToRead, &readBytes))
                {
                    return error;
                }
                m_distance.QuadPart += readBytes;
                ptr += readBytes;
                sizeToCopy -= readBytes;

                if (operation->IsCancelled())
                {
                    return ERROR_CANCELLED;
                }
            }
            return 0;
        }
    };

    class CProcessSystemImpl :public IProcessSystem
    {
    public:
        CProcessSystemImpl()
        {
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
            int error = 0;
            std::shared_ptr<IProcess> proc;
            DWORD desiredAccess = PROCESS_QUERY_INFORMATION | STANDARD_RIGHTS_REQUIRED | SYNCHRONIZE | PROCESS_VM_OPERATION | PROCESS_VM_READ;

            if (HANDLE hProc = OpenProcess(desiredAccess, FALSE, (DWORD)fileId.pid))
            {
                oui::ScopedGuard handlerGuard([&]() {
                    CloseHandle(hProc);
                });

                bool is32bit = CheckIs32BitProcess(hProc);

                std::vector<wchar_t> buf(4096);
                if (!GetProcessImageFileNameW(hProc, buf.data(), (DWORD)(buf.size() - 1)))
                {
                    error = GetLastError();
                }
                else
                {
                    proc = std::make_shared<CProcess>(buf.data(), hProc, is32bit);
                    handlerGuard.Release();
                }
            }
            else
            {
                error = GetLastError();
            }
            auto operation = std::make_shared<Operation<ProcessRecipientHandler_type>>(
                targetThread,
                openHandler);

            operation->ReplyWithRetain(operation, proc, error);
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

                const DWORD desiredAccess = PROCESS_QUERY_LIMITED_INFORMATION;
                if (HANDLE hProc = OpenProcess(desiredAccess, FALSE, (DWORD)info.pid))
                {
                    oui::ScopedGuard handlerGuard([&]() {
                        CloseHandle(hProc);
                    });

                    if (CheckIs32BitProcess(hProc))
                    {
                        info.pointerSize = 4;
                    }
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