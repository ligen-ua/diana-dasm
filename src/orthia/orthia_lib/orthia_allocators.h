#pragma once


#include "diana_core_cpp.h"
#include "orthia_interfaces.h"

extern "C"
{
#include "diana_patchers/diana_ultimate_patcher.h"
}

namespace orthia
{

struct PatcherHookAllocator:DianaHook_Allocator
{
    IHookAllocator * m_pHookAllocator;
public:
    PatcherHookAllocator(IHookAllocator * pHookAllocator);
};


class CProcessAllocator:public orthia::IHookAllocator
{
    HANDLE m_hProcess;
    bool m_needToFreeInDestructor;
    DWORD m_dwPageSize;

    struct AllocationInfo
    {
        OPERAND_SIZE size;
        bool executable;
        AllocationInfo(OPERAND_SIZE size_in = 0, bool executable_in = false)
            :
                size(size_in), executable(executable_in)
        {
        }
    };
    std::map<OPERAND_SIZE, AllocationInfo> m_allocations;

    OPERAND_SIZE AllocNearby(OPERAND_SIZE address, OPERAND_SIZE size);

public:
    CProcessAllocator(HANDLE hProcess, bool needToFreeInDestructor);
    ~CProcessAllocator();
    virtual void DianaHook_Alloc(OPERAND_SIZE size, 
                                 OPERAND_SIZE * pAddress,
                                 const OPERAND_SIZE * pHintAddress,
                                 int flags);
    virtual void DianaHook_Free(OPERAND_SIZE pAddress);
    void MakeExecutable();
    void Release();
};

void RemoteMakeExecutable(HANDLE hProcess, OPERAND_SIZE address, OPERAND_SIZE size);

}