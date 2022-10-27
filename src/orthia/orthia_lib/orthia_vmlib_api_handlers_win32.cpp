#include "orthia_vmlib_api_handlers_win32.h"
#include "orthia_vmlib_api_handlers.h"

namespace orthia
{

static bool Handler_NtAllocateVirtualMemory(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[7];

    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 7));

    OPERAND_SIZE baseAddress = 0, regionSize = 0;
    DI_CHECK_CPP(DianaProcessor_GetMemValue(pCallContext,
                                            GET_REG_SS,
                                            args[2],
                                            pCallContext->m_context.iMainMode_addressSize,
                                            &baseAddress,
                                            0,
                                            reg_SS));

    DI_CHECK_CPP(DianaProcessor_GetMemValue(pCallContext,
                                        GET_REG_SS,
                                        args[4],
                                        pCallContext->m_context.iMainMode_addressSize,
                                        &regionSize,
                                        0,
                                        reg_SS));

    ULONG allocType = (ULONG)args[5];
    OPERAND_SIZE allocated = 0;
    OPERAND_SIZE allocatedSize = 0;
    OPERAND_SIZE status = 0;
    if (!parameters.pProcessor->GetCache().VmAlloc(parameters.pAddressSpace, 
                                                    baseAddress, 
                                                    regionSize, 
                                                    &allocated, 
                                                    &allocatedSize,
                                                    (allocType&MEM_COMMIT) == MEM_COMMIT))
    {
        status = 0xC0000018; // STATUS_CONFLICTING_ADDRESSES
    }
    else
    {
        DI_CHECK_CPP(DianaProcessor_SetMemValue(pCallContext,
                                            GET_REG_SS,
                                            args[2],
                                            pCallContext->m_context.iMainMode_addressSize,
                                            &allocated,
                                            0,
                                            reg_SS));

        DI_CHECK_CPP(DianaProcessor_SetMemValue(pCallContext,
                                        GET_REG_SS,
                                        args[4],
                                        pCallContext->m_context.iMainMode_addressSize,
                                        &allocatedSize,
                                        0,
                                        reg_SS));
    }

    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 7, status));
    return true;
}

static bool Handler_NtFreeVirtualMemory(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[5];

    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 5));

    OPERAND_SIZE baseAddress = 0, regionSize = 0;
    DI_CHECK_CPP(DianaProcessor_GetMemValue(pCallContext,
                                            GET_REG_SS,
                                            args[2],
                                            pCallContext->m_context.iMainMode_addressSize,
                                            &baseAddress,
                                            0,
                                            reg_SS));

    DI_CHECK_CPP(DianaProcessor_GetMemValue(pCallContext,
                                        GET_REG_SS,
                                        args[3],
                                        pCallContext->m_context.iMainMode_addressSize,
                                        &regionSize,
                                        0,
                                        reg_SS));
    ULONG freeType = (ULONG)args[4];
    OPERAND_SIZE allocated = 0;
    OPERAND_SIZE allocatedSize = 0;
    OPERAND_SIZE status = 0;
    if (!parameters.pProcessor->GetCache().VmFree(parameters.pAddressSpace, 
                                                    baseAddress, 
                                                    &allocated, 
                                                    &allocatedSize,
                                                    freeType == MEM_RELEASE))
    {
        status = 0xC0000018; // STATUS_CONFLICTING_ADDRESSES
    }
    else
    {
        DI_CHECK_CPP(DianaProcessor_SetMemValue(pCallContext,
                                            GET_REG_SS,
                                            args[2],
                                            pCallContext->m_context.iMainMode_addressSize,
                                            &allocated,
                                            0,
                                            reg_SS));

        DI_CHECK_CPP(DianaProcessor_SetMemValue(pCallContext,
                                        GET_REG_SS,
                                        args[3],
                                        pCallContext->m_context.iMainMode_addressSize,
                                        &regionSize,
                                        0,
                                        reg_SS));
    }

    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 5, status));
    return true;
}

//_RtlEncodePointer@4:
//776E9700 8B FF            mov         edi,edi 
//776E9702 55               push        ebp  
//776E9703 8B EC            mov         ebp,esp 
//776E9705 51               push        ecx  
//776E9706 6A 00            push        0    
//776E9708 6A 04            push        4    
//776E970A 8D 45 FC         lea         eax,[ebp-4] 
//776E970D 50               push        eax  
//776E970E 6A 24            push        24h  
//776E9710 6A FF            push        0FFFFFFFFh 
//776E9712 E8 69 2D FF FF   call        _ZwQueryInformationProcess@20 (776DC480h) 
static bool Handler_NtQueryInformationProcess(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE status = 0;
    OPERAND_SIZE args[6];

    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 6));

    if (args[2] != 0x24)
    {
        return false;
    }
    OPERAND_SIZE processInformation = 0;
    DI_CHECK_CPP(DianaProcessor_SetMemValue(pCallContext,
                                            GET_REG_SS,
                                            args[3],
                                            args[4],
                                            &processInformation,
                                            0,
                                            reg_SS));

    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 6, status));
    return true;
}


//759C0369 6A 04            push        4    
//759C036B 6A 08            push        8    
//759C036D 8D 4D F8         lea         ecx,[ebp-8] 
//759C0370 51               push        ecx  
//759C0371 8D 4D F0         lea         ecx,[ebp-10h] 
//759C0374 51               push        ecx  
//759C0375 50               push        eax  
//759C0376 FF 15 C0 60 A7 75 call        dword ptr [__imp__NtQueryVolumeInformationFile@20 (75A760C0h)] 
static bool Handler_NtQueryVolumeInformationFile(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE status = 0;
    OPERAND_SIZE args[6];

    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 6));

    if ((ULONG)args[5] != 0x4 || (ULONG)args[4] != 0x8)
    {
        return false;
    }
    OPERAND_SIZE bytesWrote = 0;
    DI_UINT32 result[] = {0x00000050, 0x00020000};
    DI_CHECK_CPP(DianaProcessor_WriteMemory(pCallContext,
                                            GET_REG_DS,
                                            args[3],
                                            result,
                                            sizeof(result),
                                            &bytesWrote,
                                            0,
                                            reg_DS));

    DI_UINT32 status_32[] = {0x00000000, 0x00000008};
    DI_UINT64 status_64[] = {0x00000000, 0x00000008};
    DI_UINT64 sizeOfStatus = 0;
    void * pStatus = 0;
    if (pCallContext->m_context.iAMD64Mode)
    {
        pStatus = status_64;
        sizeOfStatus = sizeof(status_64);
    }
    else
    {
        pStatus = status_32;
        sizeOfStatus = sizeof(status_32);
    }
    DI_CHECK_CPP(DianaProcessor_WriteMemory(pCallContext,
                                            GET_REG_DS,
                                            args[2],
                                            pStatus,
                                            sizeOfStatus,
                                            &bytesWrote,
                                            0,
                                            reg_DS));

    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 6, status));
    return true;
}

//BOOLEAN
//_HeapValidate (
//    PVOID HeapHandle,
//    IN ULONG Flags,
//    IN PVOID BaseAddress
//    )
//Handler_HeapValidate
static bool Handler_HeapValidate(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[4];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 4));
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 4, 1));
    return true;
}

static bool Handler_NtOpenKey(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[4];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 4));
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 4, 0xc0000022));
    return true;
}
static bool Handler_NtQueryValueKey(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[7];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 7));
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 7, 0xc0000022));
    return true;
}
static bool Handler_NtClose(CommonHandlerParameters & parameters)
{
    return false;
}

 //NTSTATUS NtGetNlsSectionPtr(DWORD NlsType, 
 //                           DWORD CodePage, 
 //                           PVOID *SectionPointer, 
 //                           PULONG SectionSize) 
static bool Handler_NtQueryInformationToken(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[5];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 5));
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 5, 0xc0000022));
    return true;
}

static bool Handler_NtGetNlsSectionPtr(CommonHandlerParameters & parameters)
{
    OPERAND_SIZE args[5];
    DianaProcessor * pCallContext = parameters.pProcessor->GetSelf();
    DI_CHECK_CPP(QueryStdHookArgs(parameters.pProcessor, 0, args, 5));
    DI_CHECK_CPP(SkipStdFunctionCall(parameters.pProcessor, args, 5, 0xc0000022));
    return true;
}
///---
CWin32APIHandlerPopulator::CWin32APIHandlerPopulator()
{
}

static
void RegisterFunction(ICommonAPIHandlerStorage * pCommonAPIHandlerStorage,
                              IAPIHandlerDebugInterface * pDebugInterface,
                              OPERAND_SIZE module,
                              const char * pDllName, 
                              const char * pFunctionName,
                              CommonHandlerFunction_type pHandler)
{
    OPERAND_SIZE functionAddress = pDebugInterface->QueryFunctionAddress(module, pDllName, pFunctionName);
    if (functionAddress)
    {
        pCommonAPIHandlerStorage->RegisterHandler(functionAddress, pHandler);
    }

}

void CWin32APIHandlerPopulator::RegisterHandlers(ICommonAPIHandlerStorage * pCommonAPIHandlerStorage,
                                                 IAPIHandlerDebugInterface * pDebugInterface,
                                                 int dianaMode)
{
    OPERAND_SIZE ntDll = pDebugInterface->QueryModule("ntdll.dll");
    if (ntDll)
    {
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtAllocateVirtualMemory", 
                         Handler_NtAllocateVirtualMemory);
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtFreeVirtualMemory", 
                         Handler_NtFreeVirtualMemory);
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtQueryInformationProcess", 
                         Handler_NtQueryInformationProcess);
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtQueryVolumeInformationFile", 
                         Handler_NtQueryVolumeInformationFile);


        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtOpenKey", 
                         Handler_NtOpenKey);

        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtQueryValueKey", 
                         Handler_NtQueryValueKey);

        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtClose", 
                         Handler_NtClose);

        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtGetNlsSectionPtr", 
                         Handler_NtGetNlsSectionPtr);
        
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         ntDll, "ntdll.dll", "NtQueryInformationToken", 
                         Handler_NtQueryInformationToken);
    }
    OPERAND_SIZE kernel32 = pDebugInterface->QueryModule("kernel32.dll");
    if (kernel32)
    {
        RegisterFunction(pCommonAPIHandlerStorage, pDebugInterface, 
                         kernel32, "kernel32.dll", "HeapValidate", 
                         Handler_HeapValidate);

    }
}


}
