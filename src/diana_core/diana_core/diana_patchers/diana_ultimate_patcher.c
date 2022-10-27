#include "diana_ultimate_patcher.h"
#include "diana_analyze.h"
#include "diana_ultimate_patcher_impl.h"

int DianaHook_PatchStream32(DianaHook_InternalMessage * pMessage);
int DianaHook_PatchStream64(DianaHook_InternalMessage * pMessage);


void DianaHook_Allocator_Init(DianaHook_Allocator * pDianaHookAllocator,
                              DianaHook_Alloc_type alloc,
                              DianaHook_Free_type free)
{
    pDianaHookAllocator->alloc = alloc;
    pDianaHookAllocator->free = free;
}
void DianaHook_TargetMemoryProvider_Init(DianaHook_TargetMemoryProvider * pDianaHookProvider,
                                         DianaReadWriteRandomStream * pReadWriteStream,
                                         DianaHook_Allocator * pAllocator)
{
    pDianaHookProvider->pReadWriteStream = pReadWriteStream;
    pDianaHookProvider->pAllocator = pAllocator;
}
int DianaHook_PatchStream(DianaHook_TargetMemoryProvider * pTargetMemoryProvider,
                          int processorMode,
                          OPERAND_SIZE addressToHook,
                          OPERAND_SIZE hookFunction,
                          DianaHook_CustomOptions * pCustomOptions)
{
    int result = 0;
    DianaHook_InternalMessage * pMessage = 0;

    pMessage = (DianaHook_InternalMessage *)DIANA_MALLOC(sizeof(DianaHook_InternalMessage));
    DI_CHECK_ALLOC(pMessage);

    DIANA_MEMSET(pMessage, 0, sizeof(DianaHook_InternalMessage));
    pMessage->pReadWriteStream = pTargetMemoryProvider->pReadWriteStream;
    pMessage->pAllocator = pTargetMemoryProvider->pAllocator;
    pMessage->processorMode = processorMode;
    pMessage->pCustomOptions = pCustomOptions;
    pMessage->addressToHook = addressToHook;
    pMessage->hookFunction = hookFunction;
    if (pCustomOptions)
    {
        pMessage->patchContext = pCustomOptions->patchContext;
    }
    if (pCustomOptions && (pCustomOptions->flags & DIANA_HOOK_CUSTOM_OPTION_SAVE_ORIGINAL_FP))
    {
        pMessage->originalFunctionPointer = pCustomOptions->originalFunctionPointer;
    }
    else
    {
        pMessage->originalFunctionPointer = (DI_OPERAND_SIZE)-1;
    }
    switch (processorMode)
    {
    case DIANA_MODE32:
        result = DianaHook_PatchStream32(pMessage);
        break;
    case DIANA_MODE64:
        result = DianaHook_PatchStream64(pMessage);
        break;
    case DIANA_MODE16:
        result = DI_ERROR_NOT_IMPLEMENTED;
        break;
    default:
        result = DI_ERROR;
    }
    if (pMessage)
    {
        DIANA_FREE(pMessage);
    }
    return result;
}