#ifndef DIANA_PATCHERS_H
#define DIANA_PATCHERS_H

#include "diana_core.h"

#ifdef DIANA_CFG_I386


#define DIANA_PATCHER_GET_FLAGS(pOriginalESP) (*(unsigned int *)pOriginalESP)
#define DIANA_PATCHER_GET_EAX(pOriginalESP)   (*((unsigned int *)pOriginalESP+8))
#define DIANA_PATCHER_GET_ECX(pOriginalESP)   (*((unsigned int *)pOriginalESP+7))
#define DIANA_PATCHER_GET_EDX(pOriginalESP)   (*((unsigned int *)pOriginalESP+6))
#define DIANA_PATCHER_GET_EBX(pOriginalESP)   (*((unsigned int *)pOriginalESP+5))
#define DIANA_PATCHER_GET_ESP(pOriginalESP)   (*((unsigned int *)pOriginalESP+4))
#define DIANA_PATCHER_GET_EBP(pOriginalESP)   (*((unsigned int *)pOriginalESP+3))
#define DIANA_PATCHER_GET_ESI(pOriginalESP)   (*((unsigned int *)pOriginalESP+2))
#define DIANA_PATCHER_GET_EDI(pOriginalESP)   (*((unsigned int *)pOriginalESP+1))

#define DIANA_RET_ARG(espVal, num, type)  (*(type*)((unsigned long*)espVal+num))

// forward declaration
struct DianaPatcher_RetHook32_;

typedef void (__stdcall *Diana_PatchHandlerFunction2_type)(void * pContext, 
                                                           void * pOriginalESP,
                                                           void * pInputRegisters);
typedef void (__stdcall *Diana_PatchHandlerFunction_type)(void * pContext, void * pOriginalESP);
typedef void (__stdcall *Diana_RetHandlerFunction_type)(struct DianaPatcher_RetHook32_ * pContext, 
                                                        void * pRegistersSet);

typedef struct DianaPatcher_RetHook32_
{
    Diana_RetHandlerFunction_type pFunction;
    void * pContext;
    Diana_Allocator * pAllocator;
    void * pOriginalESP;
    unsigned int * pCopiedArgs;
    void * pCallContext;
    unsigned char retStub[40];
}DianaPatcher_RetHook32;

#ifdef _MSC_VER 
#pragma warning(disable:4731)
#endif

// should be used with Diana_PatchSomething32 handlers
#define DIANA_RETURN(sizeN, x) {\
    __asm mov eax, x \
    __asm mov esp, ebp \
    __asm pop ebp \
    __asm add esp, 0xC \
    __asm ret sizeN }

// should be used with Diana_PatchSomething32Ex handlers
#define DIANA_RETURN_EX(sizeN, x) {\
    __asm mov eax, x \
    __asm mov esp, ebp \
    __asm pop ebp \
    __asm mov esp, ebp \
    __asm pop ebp \
    __asm add esp, 0xc \
    __asm ret sizeN }


// [1..N]
#define DIANA_ARG(espVal, num, type)  (*(type*)((unsigned long*)espVal+num))
#define DIANA_RET(espVal)  (*((unsigned long*)espVal))
#define DIANA_RET_PTR(espVal)  (*((void**)espVal))

// OLD: only patches, do not saves input registers, cannot allocate call context, 
int Diana_PatchSomething32(void * pPlaceToHook,
                           DIANA_SIZE_T size,
                           Diana_PatchHandlerFunction_type pPatchFnc,
                           void * pPatchContext,
                           Diana_Allocator * pAllocator,
                           void ** ppOriginalFunction); // OUT

// Diana_PatchSomething32Ex:
// 
int Diana_SetupRet(Diana_Allocator * pAllocator,
                   void * pOriginalESP,
                   Diana_RetHandlerFunction_type pFunction, 
                   void * pContext,
                   int paramsCount,
                   int callContextSizeInBytes,
                   void ** ppCallContext); // OUT

int Diana_PatchSomethingEx(void * pPlaceToHook,
                           DIANA_SIZE_T size,
                           Diana_PatchHandlerFunction2_type pPatchFnc, 
                           void * pPatchContext,         
                           Diana_Allocator * pAllocator, 
                           void ** ppOriginalFunction);  // OUT


#endif

#endif