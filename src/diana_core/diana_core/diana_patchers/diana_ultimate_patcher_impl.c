#include "diana_ultimate_patcher.h"
#include "diana_ultimate_patcher_impl.h"

int DianaHook_AllocateMetainfo(DianaHook_InternalMessage * pMessage,
                               DIANA_SIZE_T structSize,
                               void ** pResult)
{
    if (structSize > DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_META)
    {
        return DI_ERROR;
    }
    if (pMessage->workBufferMetaSize > (DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_META - structSize))
    {
        return DI_ERROR;
    }
    *pResult = pMessage->workBufferMeta + pMessage->workBufferMetaSize;
    pMessage->workBufferMetaSize += structSize;
    return DI_SUCCESS;
}
int DianaHook_AllocateCmd(DianaHook_InternalMessage * pMessage,
                          DIANA_SIZE_T cmdSize,
                          void ** pResult)
{
    if (cmdSize > DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_RAW)
    {
        return DI_ERROR;
    }
    if (pMessage->workBufferRawSize > (DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_RAW - cmdSize))
    {
        return DI_ERROR;
    }
    *pResult = pMessage->workBufferRaw + pMessage->workBufferRawSize;
    pMessage->workBufferRawSize += cmdSize;
    return DI_SUCCESS;
}
OPERAND_SIZE DianaHook_Diff(OPERAND_SIZE op1, OPERAND_SIZE op2)
{
    if (op1 > op2)
    {
        return op1 - op2;
    }
    return op2 - op1;
}

void DianaHook_ClearBuffers(DianaHook_InternalMessage * pMessage)
{
    pMessage->workBufferMetaSize = 0;
    pMessage->workBufferRawSize = 0;
}
