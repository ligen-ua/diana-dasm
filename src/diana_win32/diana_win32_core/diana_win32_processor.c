#include "diana_win32_processor.h"
#include "diana_win32_exceptions.h"

// {64018619-F561-41b1-90FF-2EA1BB57CF4C}
static const DIANA_UUID g_win32ProcessorId = 
{ 0x64018619, 0xf561, 0x41b1, { 0x90, 0xff, 0x2e, 0xa1, 0xbb, 0x57, 0xcf, 0x4c } };

DianaWin32Processor * DianaWin32Processor_Cast(DianaProcessor* pThis)
{
    if (DianaBase_Match(&pThis->m_base, &g_win32ProcessorId))
        return (DianaWin32Processor * )pThis;
    return 0;
}

static
int DianaWin32Processor_InitImpl(DianaWin32Processor * pThis,
                                 DianaRandomReadWriteStream * pMemoryStream)
{
    memset(pThis, 0, sizeof(DianaWin32Processor));

    DianaWin32Stream_Init(&pThis->m_memoryStream);

    // memory allocator
    Diana_InitMAllocator(&pThis->m_memAllocator);
       
    // processor
    if (pMemoryStream)
    {
        DI_CHECK(DianaProcessor_Init(&pThis->m_processor, 
                                    pMemoryStream,
                                    &pThis->m_memAllocator.m_parent,
                                    DIANA_MODE32));
    }
    else
    {
        DI_CHECK(DianaProcessor_Init(&pThis->m_processor, 
                                    &pThis->m_memoryStream.m_parent,
                                    &pThis->m_memAllocator.m_parent,
                                    DIANA_MODE32));
    }

    DI_CHECK(DianaWin32Processor_InitProcessorExceptions(pThis));

    DianaBase_Init(&pThis->m_processor.m_base, &g_win32ProcessorId);
    return DI_SUCCESS;
}

int DianaWin32Processor_InitEx(DianaWin32Processor * pThis,
                               DianaRandomReadWriteStream * pMemoryStream,
                               OPERAND_SIZE stackLimit,
                               OPERAND_SIZE stackBase)
{
    int result = DianaWin32Processor_InitImpl(pThis, pMemoryStream);
    if (result != DI_SUCCESS)
    {
        // clean resources
    }
    pThis->m_stackLimit = stackLimit;
    pThis->m_stackBase = stackBase;
    return result;
}
int DianaWin32Processor_Init(DianaWin32Processor * pThis,
                             OPERAND_SIZE stackLimit,
                             OPERAND_SIZE stackBase)
{
    return DianaWin32Processor_InitEx(pThis, 
                                      0,
                                      stackLimit,
                                       stackBase);
}

void DianaWin32Processor_Free(DianaWin32Processor * pThis)
{
    DianaProcessor_Free(&pThis->m_processor);
}