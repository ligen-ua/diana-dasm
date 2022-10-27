#include "orthia_allocators.h"

namespace orthia
{

static
int DianaHook_Alloc(void * pThis, 
                    OPERAND_SIZE size, 
                    OPERAND_SIZE * pAddress,
                    const OPERAND_SIZE * pHintAddress,
                    int flags)
{
    try
    {
        PatcherHookAllocator * pAllocator = (PatcherHookAllocator * )pThis;
        pAllocator->m_pHookAllocator->DianaHook_Alloc(size, pAddress, pHintAddress, flags);
        return DI_SUCCESS;
    }
    catch(std::exception & ex)
    {
        &ex;
        return DI_ERROR;
    }
}

static
void DianaHook_Free(void * pThis, 
                   const OPERAND_SIZE * pAddress)
{
    try
    {
        PatcherHookAllocator * pAllocator = (PatcherHookAllocator * )pThis;
        pAllocator->m_pHookAllocator->DianaHook_Free(*pAddress);
    }
    catch(std::exception & ex)
    {
        &ex;
    }
}

PatcherHookAllocator::PatcherHookAllocator(IHookAllocator * pHookAllocator)
    :
        m_pHookAllocator(pHookAllocator)
{
    DianaHook_Allocator_Init(this,
                              &DianaHook_Alloc,
                              &DianaHook_Free);
}

void RemoteMakeExecutable(HANDLE hProcess, OPERAND_SIZE address, OPERAND_SIZE size)
{
    DWORD oldProtect = 0;
    if (!VirtualProtectEx(hProcess, 
                        (LPVOID)address,
                        (SIZE_T)size,
                        PAGE_EXECUTE_READ,
                        &oldProtect))
    {
            if (!VirtualProtectEx(hProcess, 
                        (LPVOID)address,
                        (SIZE_T)100,
                        PAGE_EXECUTE_READ,
                        &oldProtect))
            {
        ORTHIA_THROW_WIN32("VirtualProtectEx failed");
            }
    }
    if (!FlushInstructionCache(hProcess, (LPVOID)address, (SIZE_T)size))
    {
        ORTHIA_THROW_WIN32("FlushInstructionCache failed");
    }
}
// CProcessAllocator
CProcessAllocator::CProcessAllocator(HANDLE hProcess, bool needToFreeInDestructor)
    :
        m_hProcess(hProcess),
        m_needToFreeInDestructor(needToFreeInDestructor)
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    m_dwPageSize = sysInfo.dwPageSize;
}
CProcessAllocator::~CProcessAllocator()
{
    if (m_needToFreeInDestructor)
    {
        for(std::map<OPERAND_SIZE, AllocationInfo>::const_iterator it = m_allocations.begin(), 
                                                                   it_end = m_allocations.end();
            it != it_end;   
            ++it)
        {
            VirtualFreeEx(m_hProcess, 
                          (PVOID)it->first,
                          0,
                          MEM_RELEASE);
        }
    }
}
OPERAND_SIZE CProcessAllocator::AllocNearby(OPERAND_SIZE address, OPERAND_SIZE size)
{
    const OPERAND_SIZE maxDiff = 0x7FFFFFFFULL;
    if (size >= maxDiff)
    {
        return 0;
    }
    OPERAND_SIZE lowBoundary = 0;
    if (address <= maxDiff)
    {
        lowBoundary = m_dwPageSize*64;
    }
    else
    {
        lowBoundary = address - maxDiff + m_dwPageSize;
    }
    if (lowBoundary%m_dwPageSize)
    {
        lowBoundary -= lowBoundary%m_dwPageSize;
    }


    OPERAND_SIZE highBoundary = 0;
    if (address >= (0xFFFFFFFFFFFFFFFFULL - maxDiff))
    {
        highBoundary = 0xFFFFFFFFFFFFFFFFULL - size;
    }
    else
    {
        highBoundary = address + maxDiff - size;
    }
    if (highBoundary%m_dwPageSize)
    {
        highBoundary -= highBoundary%m_dwPageSize;
    }

    MEMORY_BASIC_INFORMATION memInfo;
    OPERAND_SIZE freeStart = 0;
    for(SIZE_T offset = (SIZE_T)lowBoundary;; 
        )
    {
        size_t bytes = VirtualQueryEx(m_hProcess,
                     (char*)offset, 
                     &memInfo,
                     sizeof(memInfo));
        if (!bytes)
        {
            return 0;
        }

        if ((OPERAND_SIZE)memInfo.BaseAddress > freeStart && freeStart >= lowBoundary)
        {
            OPERAND_SIZE freeSize = (OPERAND_SIZE)memInfo.BaseAddress - freeStart;
        
            if (freeSize >= size)
            {
                // try alloc
                OPERAND_SIZE res = (OPERAND_SIZE)VirtualAllocEx(m_hProcess, 
                                                 (PVOID)freeStart, 
                                                 (SIZE_T)size, 
                                                 MEM_COMMIT|MEM_RESERVE, 
                                                 PAGE_READWRITE);
                if (res)
                {
                    return res;
                }
            }
        }

        freeStart = offset;
        if (freeStart >= highBoundary)
        {
            return 0;
        }
        offset = (OPERAND_SIZE)memInfo.BaseAddress + (OPERAND_SIZE)memInfo.RegionSize;
    } 
}
void CProcessAllocator::DianaHook_Alloc(OPERAND_SIZE size, 
                                        OPERAND_SIZE * pAddress,
                                        const OPERAND_SIZE * pHintAddress,
                                        int flags)
{
    DWORD flagsToPass = 0;
    if (pHintAddress)
    {
        *pAddress = AllocNearby(*pHintAddress, size);   
    }
    else
    {
        flagsToPass = MEM_TOP_DOWN;
    }
    if (!*pAddress)
    {
        *pAddress = (OPERAND_SIZE)VirtualAllocEx(m_hProcess, 
                                                 NULL, 
                                                 (SIZE_T)size, 
                                                 MEM_COMMIT|MEM_RESERVE|flagsToPass, 
                                                 PAGE_READWRITE);
        if (!*pAddress)
        {
            ORTHIA_THROW_WIN32("Can't alloc remote memory");
        }
    }
    m_allocations[*pAddress] = AllocationInfo(size, (flags & DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE)!=0);
}
void CProcessAllocator::DianaHook_Free(OPERAND_SIZE pAddress)
{
    VirtualFreeEx(m_hProcess, 
                  (PVOID)pAddress,
                  0,
                  MEM_RELEASE);
    m_allocations.erase(pAddress);
}
void CProcessAllocator::MakeExecutable()
{
    for(std::map<OPERAND_SIZE, AllocationInfo>::const_iterator it = m_allocations.begin(), 
                                                         it_end = m_allocations.end();
        it != it_end;
        ++it)
    {
        if (it->second.executable)
        {
            RemoteMakeExecutable(m_hProcess, it->first, it->second.size);
        }
    }
}
void CProcessAllocator::Release()
{
    m_needToFreeInDestructor = false;
}

}