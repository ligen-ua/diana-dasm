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

/* x86 and x64 code: processorMode is DIANA_MODE32 or DIANA_MODE64. The same as DianaHook_PatchStreamEx
   with DIANA_ARCH_X86 */
int DianaHook_PatchStream(DianaHook_TargetMemoryProvider * pTargetMemoryProvider,
                          int processorMode,
                          OPERAND_SIZE addressToHook,
                          OPERAND_SIZE hookFunction,
                          DianaHook_CustomOptions * pCustomOptions);

/* arch is DIANA_ARCH_*:
   - DIANA_ARCH_X86, DIANA_MODE32 or DIANA_MODE64: as DianaHook_PatchStream.
   - DIANA_ARCH_AARCH64, DIANA_MODE64: hookFunction(x0 = patchContext, x1 = the sp of the hooked function,
     x2 = the saved registers: x0..x18 at [0..18], nzcv [19], x29 [20], x30 [21], 8 bytes each; a change
     there is what the function gets). The SIMD registers are not saved. The hooked function starts with
     "b <stub>" when the stub is within 128 MB (DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP: never), otherwise with
     "ldr x16, #8; br x16; .quad <stub>". The stub jumps back with "b" when it can: on Linux pages with BTI,
     the "br x16" back into the function faults, so allocate near the hint. An entry "b" is followed unless
     DIANA_HOOK_CUSTOM_OPTION_DONT_FOLLOW_JUMPS.
   - DIANA_ARCH_ARM32, DIANA_ARCH_THUMB, DIANA_MODE32: hookFunction(r0 = patchContext, r1 = the sp of the hooked
     function, r2 = the saved registers: cpsr then r0..r12, lr). The hooked function starts with "b <stub>"
     (A32, stub within 32 MB) or a pc-relative load of the stub. The stub reaches hookFunction with "blx", so a
     Thumb handler needs bit 0 of hookFunction set. For THUMB, addressToHook's bit 0 is ignored and the saved
     original function pointer keeps it set. Among the overwritten instructions b, bl, b<cond> and cbz/cbnz are
     moved; a pc-relative load, adr or an IT block there is refused (DI_ERROR_NOT_IMPLEMENTED).
   ARM: the caller flushes the instruction cache, in pRandomWrite for example. In a DIANA_NO_ARM build every
   ARM architecture gets DI_UNSUPPORTED */
int DianaHook_PatchStreamEx(DianaHook_TargetMemoryProvider * pTargetMemoryProvider,
                            int processorMode,
                            int arch,
                            OPERAND_SIZE addressToHook,
                            OPERAND_SIZE hookFunction,
                            DianaHook_CustomOptions * pCustomOptions);

#endif