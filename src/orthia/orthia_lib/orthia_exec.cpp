#include "orthia_exec.h"
#include "orthia_module_manager.h"
#include "orthia_memory_cache.h"
#include "diana_core_cpp.h"

extern "C"
{
#include "diana_core_win32_context.h"
#include "diana_processor/diana_processor_win32_context.h"
#include "diana_processor/diana_processor_cmd_n.h"
}

#include "orthia_memory_cache.h"
#include "orthia_plugin_interfaces.h"


extern "C"
{
#include "diana_processor/diana_processor_commands.h"
#include "diana_processor/diana_processor_core_impl.h"
#include "diana_core_gen_tags.h"
#include "diana_processor/diana_processor_cmd_j.h"
}
#include "intrin.h"

namespace orthia
{

static 
int orthia_lsl(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(dest);

    dest = 0x00007c00;
    DI_MEM_SET_DEST(dest); 
    SET_FLAG_ZF;
    DI_PROC_END;
}

static 
int orthia_rdtsc(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    ULARGE_INTEGER res;
    res.QuadPart = __rdtsc();
    
    SET_REG_EDX(res.HighPart);
    SET_REG_EAX(res.QuadPart);
    return DI_SUCCESS;
}
static 
int orthia_rdtscp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    ULARGE_INTEGER res;
    res.QuadPart = __rdtsc();
    
    SET_REG_RDX(res.HighPart);
    SET_REG_RAX(res.QuadPart);
    SET_REG_RCX(0x00007c00);
    return DI_SUCCESS;
}

static 
int orthia_jmp(struct _dianaContext * pDianaContext,
                        DianaProcessor * pCallContext)
{
    OPERAND_SIZE oldRIP = GET_REG_RIP;
    OPERAND_SIZE oldCS = GET_REG_CS;
    DI_CHECK(Diana_Call_jmp(pDianaContext,
                            pCallContext));
    if (pCallContext->m_initialDianaMode == DIANA_MODE32)
    {
        if (!pDianaContext->iAMD64Mode &&
            oldCS != 0x33 && 
            GET_REG_CS == 0x33)
        {
            // jump to x64
            // Diana_InitContext(pDianaContext, DIANA_MODE64);
            DI_JUMP_TO_RIP(oldRIP);
            return DI_ERROR_NOT_IMPLEMENTED;
        }
        if (pDianaContext->iAMD64Mode &&
            oldCS == 0x33 && 
            GET_REG_CS != 0x33)
        {
            // jump to x32
            // Diana_InitContext(pDianaContext, DIANA_MODE32);
            DI_JUMP_TO_RIP(oldRIP);
            return DI_ERROR_NOT_IMPLEMENTED;
        }
    }
    return DI_SUCCESS;
}

static int DianaProcessorCustomCommandProvider(struct _dianaProcessor * pProcessor,
                                             void * pCustomProviderContext,
                                             const DianaGroupInfo * pGroupInfo,
                                             DianaProcessorCommand_type * ppCommand)
{
    *ppCommand = 0;

    CProcessor * pOrthiaProcessor = (CProcessor * )pCustomProviderContext;
    DianaProcessor * pCallContext = pOrthiaProcessor->GetSelf();

    bool apiHandled = false;
    DI_CHECK(pOrthiaProcessor->HandleAPI(GET_REG_RIP, &apiHandled));
    if (apiHandled)
    {
        *ppCommand = Diana_Call_nop;
        return DI_SUCCESS;
    }
    switch (pGroupInfo->m_commandId)
    {
    case diana_cmd_lsl:
        *ppCommand = orthia_lsl;
        break;
    case diana_cmd_rdtsc:
        *ppCommand = orthia_rdtsc;
        break;
    case diana_cmd_rdtscp:
        *ppCommand = orthia_rdtscp;
        break;
    case diana_cmd_jmp:
        *ppCommand = orthia_jmp;
        break;
    }
    return DI_SUCCESS;
}
// CProcessor
void CProcessor::InitProcessor()
{
    DI_CHECK_CPP(DianaProcessor_Init(&m_processor, 
                                     &m_memoryStream,
                                     &m_defaultAllocator.GetAllocator()->m_parent,
                                      m_mode));
    DianaProcessor_InitCustomCommandProvider(&m_processor, 
                                             DianaProcessorCustomCommandProvider,
                                             this);
}

CProcessor::CProcessor(orthia::IMemoryReader * pMemoryReader,
                       int mode,
                       orthia::Ptr<IAPIHandler> pAPIHandler)
    :
        diana::CBaseProcessor(mode),
        m_cache(pMemoryReader),
        m_memoryStream(&m_cache),
        m_pAPIHandler(pAPIHandler)
        
{
}
int CProcessor::HandleAPI(OPERAND_SIZE rip, bool * pApiHandled)
{
    *pApiHandled = false;
    try
    {
        if (!m_pAPIHandler)
        {
            return DI_SUCCESS;
        }
        *pApiHandled = m_pAPIHandler->HandleAPI(rip, this);
        return DI_SUCCESS;
    }
    catch(std::exception & )
    {
        return DI_ERROR;
    }
}
    
CMemoryStorageOfModifiedData & CProcessor::GetCache()
{
    return m_cache;
}
// exec
int Exec(orthia::IMemoryReader * pMemoryReader,
          int mode,
          Diana_Processor_Registers_Context * pContext,
          long long commandsCount,
          CMemoryStorageOfModifiedData & allWrites,
          IDebugger * pDebugger,
          long long * pCommandsCount,
          orthia::Ptr<IAPIHandler> pAPIHandler,
          std::vector<OPERAND_SIZE> * pCallStack)
{
    CProcessor proc(pMemoryReader, mode, pAPIHandler);
    proc.Init();
    DI_CHECK_CPP(DianaProcessor_InitContext(proc.GetSelf(), pContext));

    //unsigned long long aclayers = (unsigned long long)GetModuleHandle(L"AcLayers.dll"); 
    //proc.GetCache().AddMemoryWriteBreakPoint(aclayers + 0x269354);

    orthia::CGenericInterruptChecker checker(pDebugger);
    int result = proc.Exec(commandsCount, &checker, pCommandsCount);
    if (result && result != DI_END)
    {
        if (pCallStack)
        {
            pCallStack->clear();
            proc.QueryCallStack(pCallStack, 10);
        }
    }
    allWrites.Swap(proc.GetCache());
    DI_CHECK_CPP(DianaProcessor_QueryContext(proc.GetSelf(), pContext));
    return result;
}


}