#ifndef DIANA_ULTIMATE_PATCHER_H
#define DIANA_ULTIMATE_PATCHER_H

#include "diana_core.h"

#define DIANA_HOOK_FLASG_ALLOCATE_EXECUTABLE              1

typedef int ( * DianaHook_Alloc_type)(void * pThis, 
                                      OPERAND_SIZE size, 
                                      OPERAND_SIZE * pAddress,
                                      const OPERAND_SIZE * pHintAddress,
                                      int flags);
typedef void ( * DianaHook_Free_type)(void * pThis, 
                                      const OPERAND_SIZE * pAddress);

typedef struct _DianaHook_Allocator
{
    DianaHook_Alloc_type alloc;
    DianaHook_Free_type free;
}DianaHook_Allocator;

void DianaHook_Allocator_Init(DianaHook_Allocator * pDianaHookAllocator,
                              DianaHook_Alloc_type alloc,
                              DianaHook_Free_type free);

typedef struct _DianaHook_TargetMemoryProvider
{
    DianaReadWriteRandomStream * pReadWriteStream;
    DianaHook_Allocator * pAllocator;
}DianaHook_TargetMemoryProvider;

void DianaHook_TargetMemoryProvider_Init(DianaHook_TargetMemoryProvider * pDianaHookProvider,
                                         DianaReadWriteRandomStream * pReadWriteStream,
                                         DianaHook_Allocator * pAllocator);


#define DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP              0x01
#define DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS        0x02
#define DIANA_HOOK_CUSTOM_OPTION_PUT_NEAR_JMP             0x04
#define DIANA_HOOK_CUSTOM_OPTION_TEST_SINGLE_PUSH         0x08
#define DIANA_HOOK_CUSTOM_OPTION_SAVE_ORIGINAL_FP         0x10

typedef struct _DianaHook_CustomOptions
{
    OPERAND_SIZE originalFunctionPointer;
    int flags;
    OPERAND_SIZE patchContext;
}DianaHook_CustomOptions;

int DianaHook_PatchStream(DianaHook_TargetMemoryProvider * pTargetMemoryProvider,
                          int processorMode,
                          OPERAND_SIZE addressToHook,
                          OPERAND_SIZE hookFunction,
                          DianaHook_CustomOptions * pCustomOptions);

#endif