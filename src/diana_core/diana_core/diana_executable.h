#ifndef DIANA_EXECUTABLE_H
#define DIANA_EXECUTABLE_H

#include "diana_pe.h"
#include "diana_elf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DIANA_EXECUTABLE_TYPE_NONE  0
#define DIANA_EXECUTABLE_TYPE_PE    1
#define DIANA_EXECUTABLE_TYPE_ELF   2

typedef struct _DianaExecutable
{
    int type;              /* DIANA_EXECUTABLE_TYPE_PE or DIANA_EXECUTABLE_TYPE_ELF */
    int dianaMode;         /* DIANA_MODE32 or DIANA_MODE64 */
    OPERAND_SIZE sizeOfModule;
    union
    {
        Diana_PeFile  peFile;
        Diana_ElfFile elfFile;
    } u;
} DianaExecutable;



int DianaExecutable_Init(DianaExecutable * pPeFile,
                     DianaMovableReadStream * pStream,
                     OPERAND_SIZE sizeOfFile,
                     int flags);

void DianaExecutable_Free(DianaExecutable* pExe);

/* Unified QueryExports - forwards to DianaPeFile_QueryExports or DianaElfFile_QueryExports */
int DianaExecutable_QueryExports(DianaExecutable* pExe,
    DianaMovableReadStream* pStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags);

/* Unified QueryImports - forwards to DianaPeFile_QueryImports or DianaElfFile_QueryImports */
int DianaExecutable_QueryImports(DianaExecutable* pExe,
    OPERAND_SIZE address,
    DianaReadWriteRandomStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags,
    int importFlags);

/* Unified QueryTLSCallbacks - PE only; returns DI_SUCCESS with 0 callbacks for ELF */
int DianaExecutable_QueryTLSCallbacks(DianaExecutable* pExe,
    OPERAND_SIZE address,
    DianaReadWriteRandomStream* pOutStream,
    void** ppTlsCallbacks,
    int* callbacksCount,
    OPERAND_SIZE* pAddressOfTLSIndex,
    int streamFlags);

#ifdef __cplusplus
}
#endif

#endif /* DIANA_EXECUTABLE_H */
