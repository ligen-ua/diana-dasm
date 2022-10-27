#ifndef DIANA_ULTIMATE_PATCHER_IMPL_H
#define DIANA_ULTIMATE_PATCHER_IMPL_H

#define DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_META   256
#define DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_RAW   256

typedef struct _DianaHook_InternalMessage
{
    DianaReadWriteRandomStream * pReadWriteStream;
    DianaHook_Allocator * pAllocator;
    int processorMode;
    DianaHook_CustomOptions * pCustomOptions;
    OPERAND_SIZE addressToHook;
    OPERAND_SIZE patchContext;
    OPERAND_SIZE hookFunction;
    OPERAND_SIZE originalFunctionPointer;

    DianaParserResult result;
    DianaContext context;
    int jumpsCounter;
    
    // work buffer
    DIANA_SIZE_T workBufferMetaSize;
    char workBufferMeta[DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_META];

    DIANA_SIZE_T workBufferRawSize;
    char workBufferRaw[DIANAHOOK_INTERNALMESSAGE_WORK_BUFFER_SIZE_RAW];
}DianaHook_InternalMessage;


int DianaHook_AllocateMetainfo(DianaHook_InternalMessage * pMessage,
                               DIANA_SIZE_T structSize,
                               void ** pResult);
int DianaHook_AllocateCmd(DianaHook_InternalMessage * pMessage,
                          DIANA_SIZE_T cmdSize,
                          void ** pResult);

void DianaHook_ClearBuffers(DianaHook_InternalMessage * pMessage);


OPERAND_SIZE DianaHook_Diff(OPERAND_SIZE op1, OPERAND_SIZE op2);

#endif