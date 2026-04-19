#include "diana_executable.h"


int DianaExecutable_Init(DianaExecutable * pExecutable,
                     DianaMovableReadStream * pStream,
                     OPERAND_SIZE sizeOfFile,
                     int flags) 
{

    memset(pExecutable, 0, sizeof(DianaExecutable));

    if (!DianaPeFile_Init(&pExecutable->u.peFile,
        pStream,
        sizeOfFile,
        flags))
    {
        pExecutable->type = DIANA_EXECUTABLE_TYPE_PE;
        pExecutable->dianaMode = pExecutable->u.peFile.pImpl->dianaMode;
        pExecutable->sizeOfModule = pExecutable->u.peFile.pImpl->sizeOfModule;
        return DI_SUCCESS;
    }

    if (!DianaElfFile_Init(&pExecutable->u.elfFile,
        pStream,
        sizeOfFile,
        flags))
    {
        pExecutable->type = DIANA_EXECUTABLE_TYPE_ELF;
        pExecutable->dianaMode = pExecutable->u.elfFile.dianaMode;
        pExecutable->sizeOfModule = pExecutable->u.elfFile.pImpl->sizeOfModule;
        return DI_SUCCESS;
    }
    return DI_UNSUPPORTED;
}

void DianaExecutable_Free(DianaExecutable* pExe)
{
    if (!pExe)
        return;
    if (pExe->type == DIANA_EXECUTABLE_TYPE_PE)
    {
        DianaPeFile_Free(&pExe->u.peFile);
    }
    else if (pExe->type == DIANA_EXECUTABLE_TYPE_ELF)
    {
        DianaElfFile_Free(&pExe->u.elfFile);
    }
    pExe->type = DIANA_EXECUTABLE_TYPE_NONE;
}

int DianaExecutable_QueryExports(DianaExecutable* pExe,
    DianaMovableReadStream* pStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags)
{
    if (pExe->type == DIANA_EXECUTABLE_TYPE_PE)
    {
        return DianaPeFile_QueryExports(&pExe->u.peFile, pStream, pPage, pageSize, pObserver, streamFlags);
    }
    if (pExe->type == DIANA_EXECUTABLE_TYPE_ELF)
    {
        return DianaElfFile_QueryExports(&pExe->u.elfFile, pStream, pPage, pageSize, pObserver, streamFlags);
    }
    return DI_SUCCESS;
}

int DianaExecutable_QueryImports(DianaExecutable* pExe,
    OPERAND_SIZE address,
    DianaReadWriteRandomStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags,
    int importFlags)
{
    if (pExe->type == DIANA_EXECUTABLE_TYPE_PE)
    {
        return DianaPeFile_QueryImports(&pExe->u.peFile, address, pOutStream, pPage, pageSize, pObserver, streamFlags, importFlags);
    }
    if (pExe->type == DIANA_EXECUTABLE_TYPE_ELF)
    {
        return DianaElfFile_QueryImports(&pExe->u.elfFile, address, pOutStream, pPage, pageSize, pObserver, streamFlags, importFlags);
    }
    return DI_SUCCESS;
}

int DianaExecutable_QueryTLSCallbacks(DianaExecutable* pExe,
    OPERAND_SIZE address,
    DianaReadWriteRandomStream* pOutStream,
    void** ppTlsCallbacks,
    int* callbacksCount,
    OPERAND_SIZE* pAddressOfTLSIndex,
    int streamFlags)
{
    if (pExe->type == DIANA_EXECUTABLE_TYPE_PE)
    {
        return DianaPeFile_QueryTLSCallbacks(&pExe->u.peFile, address, pOutStream,
            ppTlsCallbacks, callbacksCount, pAddressOfTLSIndex, streamFlags);
    }
    /* ELF has no TLS callbacks in the PE sense - return empty result */
    if (ppTlsCallbacks)   *ppTlsCallbacks = 0;
    if (callbacksCount)   *callbacksCount = 0;
    if (pAddressOfTLSIndex) *pAddressOfTLSIndex = 0;
    return DI_SUCCESS;
}

int DianaExecutable_DetectType(const char* pData, OPERAND_SIZE size)
{
    if (size >= 4 &&
        (unsigned char)pData[0] == 0x7F &&
        pData[1] == 'E' &&
        pData[2] == 'L' &&
        pData[3] == 'F')
    {
        return DIANA_EXECUTABLE_TYPE_ELF;
    }
    if (size >= 2 &&
        (unsigned char)pData[0] == 'M' &&
        (unsigned char)pData[1] == 'Z')
    {
        return DIANA_EXECUTABLE_TYPE_PE;
    }
    return DIANA_EXECUTABLE_TYPE_NONE;
}
