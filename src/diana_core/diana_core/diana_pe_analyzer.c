#include "diana_pe_analyzer.h"

typedef struct Diana_PeAnalyzerCommonParams
{
    Diana_PeFile * pPeFile;
    DianaAnalyzeObserver * pObserver;
    Diana_InstructionsOwner * pOwner;
    void * pPage;
    int pageSize;
    Diana_Stack * pStack;
}Diana_PeAnalyzerCommonParams_type;

int DianaAnalyzeObserverOverMemory_ConvertAddressToRelative(void * pThis, 
                                                            OPERAND_SIZE address,
                                                            OPERAND_SIZE * pRelativeOffset,
                                                            int * pbInvalidPointer);

static
int ScanPage(Diana_PeAnalyzerCommonParams_type * pParams,
             int pageSize)
{
    char * p = pParams->pPage;
    char * pEnd = p + pageSize - sizeof(DIANA_SIZE_T)+1;
    if (pageSize < sizeof(DIANA_SIZE_T))
        return DI_SUCCESS;

    for(++p; p < pEnd; ++p)
    {
        DIANA_SIZE_T * pAddress = (DIANA_SIZE_T *)p;
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        if (pParams->pPeFile->pImpl->dianaMode == 4)
        {
            *pAddress = (DI_UINT32)*pAddress;
        }
        DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver, 
                                              *pAddress, 
                                              DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                              &relativeAddress, 
                                              &result));
        if (result == diaJumpNormal)
        {
            DI_CHECK(Diana_AnalyzeCodeEx(pParams->pOwner,
                                        pParams->pObserver,
                                        pParams->pPeFile->pImpl->dianaMode,
                                        relativeAddress,
                                        pParams->pPeFile->pImpl->sizeOfModule,
                                        pParams->pStack,
                                        DI_ANALYSE_BREAK_ON_INVALID_CMD));
            continue;
        }
    }
    return DI_SUCCESS;
}
static
int Diana_AnalyzeData(Diana_PeAnalyzerCommonParams_type * pParams)
{       
    DIANA_IMAGE_SECTION_HEADER * pCurrentCapturedSection = pParams->pPeFile->pImpl->pCapturedSections;
    DIANA_IMAGE_SECTION_HEADER * pCapturedSectionsEnd = pParams->pPeFile->pImpl->pCapturedSections + pParams->pPeFile->pImpl->capturedSectionCount;
    int readBytes = 0;
    DIANA_SIZE_T pageLastPointer = 0;
    for(;
        pCurrentCapturedSection != pCapturedSectionsEnd;
        ++pCurrentCapturedSection)
    {
        int result = 0;
        OPERAND_SIZE allReadBytes = 0;
        DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, 
                                                         pCurrentCapturedSection->VirtualAddress));
        for(;;)
        {
            OPERAND_SIZE sizeToRead = pParams->pageSize;
            if (allReadBytes >= pCurrentCapturedSection->Misc.VirtualSize)
            {
                break;
            }
            if (pCurrentCapturedSection->Misc.VirtualSize - allReadBytes < sizeToRead)
            {
                sizeToRead = pCurrentCapturedSection->Misc.VirtualSize - allReadBytes;
                break;
            }
            readBytes = 0;
            result = pParams->pObserver->m_pStream->parent.pReadFnc(pParams->pObserver->m_pStream, 
                                                                    (char*)pParams->pPage+sizeof(DIANA_SIZE_T), 
                                                                    (int)sizeToRead, 
                                                                    &readBytes);
            if (result || !readBytes)
                break;
            allReadBytes += readBytes;
            *(DIANA_SIZE_T*)pParams->pPage = pageLastPointer;
            DI_CHECK(ScanPage(pParams,
                              readBytes + sizeof(DIANA_SIZE_T)));
            if (readBytes >= sizeof(DIANA_SIZE_T))
            {
                pageLastPointer = *(DIANA_SIZE_T*)((char*)pParams->pPage + readBytes);
            }
            if (allReadBytes >= pCurrentCapturedSection->Misc.VirtualSize)
                break;
        }
    }
    return DI_SUCCESS;
}


static 
int Diana_AnalyzeExports(Diana_PeAnalyzerCommonParams_type * pParams)
{
    DI_UINT32 theRestNumberOfFunctions = 0;
    DI_UINT32 numberOfFunctionsPerPage = 0;
    DI_UINT32 numberOfFunctions = 0;
    OPERAND_SIZE exportsRaw  = 0;
    DIANA_IMAGE_EXPORT_DIRECTORY * pCapturedExportDirectory = (DIANA_IMAGE_EXPORT_DIRECTORY * )pParams->pPage;
    if (pParams->pageSize < sizeof(DIANA_IMAGE_EXPORT_DIRECTORY))
        return DI_INVALID_CONFIGURATION;
        
    exportsRaw = pParams->pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, exportsRaw));
    DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, pCapturedExportDirectory, sizeof(DIANA_IMAGE_EXPORT_DIRECTORY)));
    numberOfFunctions = pCapturedExportDirectory->NumberOfFunctions;
    // done work with pCapturedExportDirectory
    pCapturedExportDirectory = 0;

    if (!numberOfFunctions)
        return DI_SUCCESS;
    numberOfFunctionsPerPage = pParams->pageSize/sizeof(DI_UINT32);
    if (!numberOfFunctionsPerPage)
    {
        // buffer is too small!
        return DI_INVALID_CONFIGURATION;
    }

    theRestNumberOfFunctions = numberOfFunctions;
    while(theRestNumberOfFunctions)
    {
        DI_UINT32 i;
        DI_UINT32 numberOfFunctionsToRead = theRestNumberOfFunctions;
        DI_UINT32 * pCapturedFunctions = (DI_UINT32*)pParams->pPage;
        if (numberOfFunctionsToRead > numberOfFunctionsPerPage)
        {
            numberOfFunctionsToRead = numberOfFunctionsPerPage;
        }
        // read functions
        DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, 
                                pCapturedFunctions, 
                                numberOfFunctionsToRead * sizeof(DI_UINT32)));
        for(i = 0; i < numberOfFunctionsToRead; ++i)
        {
            if (pCapturedFunctions[i] < pParams->pPeFile->pImpl->sizeOfModule)
            {
                DI_CHECK(Diana_AnalyzeCode(pParams->pOwner,
                                            pParams->pObserver,
                                            pParams->pPeFile->pImpl->dianaMode,
                                            pCapturedFunctions[i],
                                            pParams->pPeFile->pImpl->sizeOfModule));
            }
        }
        theRestNumberOfFunctions -= numberOfFunctionsToRead;
    }
    return DI_SUCCESS;
}

static 
int Diana_PE_AnalyzePEImplUsingBuffer(Diana_PeAnalyzerCommonParams_type * pParams,
                                      int analyserFlags)
{
    // scan from entry point
    if (pParams->pPeFile->pImpl->addressOfEntryPoint)
    {
        DI_CHECK(Diana_AnalyzeCodeEx(pParams->pOwner,
                                     pParams->pObserver, 
                                     pParams->pPeFile->pImpl->dianaMode, 
                                     pParams->pPeFile->pImpl->addressOfEntryPoint, 
                                     pParams->pPeFile->pImpl->sizeOfModule,
                                     pParams->pStack,
                                     0));
    }
    {
        // scan raw pages
        DI_CHECK(Diana_AnalyzeData(pParams));
    }
    // scan exports
    DI_CHECK(Diana_AnalyzeExports(pParams));

    // scan all
    if (DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags)
    {
        OPERAND_SIZE i = 0;
        for(; i < pParams->pPeFile->pImpl->sizeOfModule; i+=4ULL)
        {
            DI_CHECK(Diana_AnalyzeCodeEx(pParams->pOwner,
                                         pParams->pObserver, 
                                         pParams->pPeFile->pImpl->dianaMode, 
                                         i, 
                                         pParams->pPeFile->pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD));
        }
    }
    return DI_SUCCESS;
}

static 
int Diana_PE_AnalyzePEImpl(Diana_PeFile * pPeFile,
                           DianaAnalyzeObserver * pObserver,
                           Diana_InstructionsOwner * pOwner,
                           int analyserFlags)
{
    // scan raw pages
    Diana_Stack stack;
    int pageSize = 0x10000;
    int status = 0;
    int stackInited = 0;
    Diana_PeAnalyzerCommonParams_type params;

    void * pPage = DIANA_MALLOC(pageSize + sizeof(OPERAND_SIZE));
    DI_CHECK_ALLOC(pPage);

    DI_CHECK_GOTO(Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    stackInited = 1;

    params.pageSize = pageSize;
    params.pPage = pPage;
    params.pPeFile = pPeFile;
    params.pOwner = pOwner;
    params.pObserver = pObserver;
    params.pStack = &stack;

    status = Diana_PE_AnalyzePEImplUsingBuffer(&params,
                                               analyserFlags);
cleanup:
    if (stackInited)
    {
        Diana_Stack_Free(&stack);
    }
    DIANA_FREE(pPage);
    return status;
}

int Diana_PE_AnalyzePE(Diana_PeFile * pPeFile,
                       DianaAnalyzeObserver * pObserver,
                       Diana_InstructionsOwner * pOwner,
                       int analyserFlags)
{
    int status = 0;
    DI_CHECK(Diana_InstructionsOwner_Init(pOwner, 
                                          pPeFile->pImpl->sizeOfModule, 
                                          0x10000));
    status = Diana_PE_AnalyzePEImpl(pPeFile,
                                    pObserver,
                                    pOwner,
                                    analyserFlags);
    if (status)
    {
        Diana_InstructionsOwner_Free(pOwner);
    }
    return status;
}


int DianaMovableReadStreamOverMemory_MoveTo(void * pThis, OPERAND_SIZE offset)
{
    DianaMovableReadStreamOverMemory * pStream = pThis;
    if (offset >= pStream->memoryStream.bufferSize)
        return DI_END_OF_STREAM;
    pStream->memoryStream.curSize = (DIANA_SIZE_T)offset;
    return DI_SUCCESS;
}
int DianaMovableReadStreamOverMemory_Read(void * pThis,
                                            void * pBuffer,
                                            int iBufferSize,
                                            int * readBytes)
{
    DianaMovableReadStreamOverMemory * pStream = pThis;
    return pStream->memoryStream.parent.parent.parent.pReadFnc(&pStream->memoryStream.parent.parent, 
                                                 pBuffer, 
                                                 iBufferSize, 
                                                 readBytes);
}

int DianaMovableReadStreamOverMemory_RandomRead(void * pThis, 
                                                OPERAND_SIZE offset,
                                                void * pBuffer, 
                                                int iBufferSize, 
                                                OPERAND_SIZE * readBytes,
                                                int flags)
{
    DianaMovableReadStreamOverMemory * pStream = pThis;
    if (flags & DIANA_ANALYZE_RANDOM_READ_ABSOLUTE)
    {
        return DI_END_OF_STREAM;
    }
    return DianaMemoryStream_RandomRead(&pStream->memoryStream.parent, 
                                        offset, 
                                        pBuffer, 
                                        iBufferSize, 
                                        readBytes);
}
                                           
void DianaMovableReadStreamOverMemory_Init(DianaMovableReadStreamOverMemory * pThis,
                                           const void * pBuffer,
                                           OPERAND_SIZE bufferSize)
{
    DianaMovableReadStream_Init(&pThis->stream, 
                                DianaMovableReadStreamOverMemory_Read,
                                DianaMovableReadStreamOverMemory_MoveTo,
                                DianaMovableReadStreamOverMemory_RandomRead);
    Diana_InitMemoryStream(&pThis->memoryStream, 
                           (void*)pBuffer, 
                           (DIANA_SIZE_T)bufferSize);
}

int DianaAnalyzeObserverOverMemory_AnalyzeJumpAddress(void * pThis, 
                                                       OPERAND_SIZE address,
                                                       int flags,
                                                       OPERAND_SIZE * pRelativeOffset,
                                                       DianaAnalyzeAddressResult_type * pResult)
{
    DianaAnalyzeObserverOverMemory * pObserver = pThis;
    DianaMovableReadStreamOverMemory * pStream = &pObserver->stream;
    *pResult = diaJumpNormal;
    *pRelativeOffset = address;

    if (flags&DIANA_ANALYZE_ABSOLUTE_ADDRESS)
    { 
        if (address >= (OPERAND_SIZE)pStream->memoryStream.pBuffer &&
            address <= (OPERAND_SIZE)pStream->memoryStream.pBuffer + pStream->memoryStream.bufferSize)
        {
            *pRelativeOffset = address - (OPERAND_SIZE)pStream->memoryStream.pBuffer;
            *pResult = diaJumpNormal;
            return DI_SUCCESS;
        }
        *pResult = diaJumpExternal;
        return DI_SUCCESS;
    }
        
    if (address >= pStream->memoryStream.bufferSize)
    {
        *pResult = diaJumpExternal;
    }
    return DI_SUCCESS;
}

void DianaAnalyzeObserverOverMemory_Init(DianaAnalyzeObserverOverMemory * pThis,
                                         void * pPeFile,
                                         OPERAND_SIZE fileSize)
{
    DianaMovableReadStreamOverMemory_Init(&pThis->stream,
                                          pPeFile,
                                          fileSize);
    DianaAnalyzeObserver_Init(&pThis->parent,
                              &pThis->stream.stream,
                              DianaAnalyzeObserverOverMemory_AnalyzeJumpAddress
                              );
}

int Diana_PE_AnalyzePEInMemory(void * pPeFile,
                                OPERAND_SIZE fileSize,
                                Diana_InstructionsOwner * pOwner,
                                int analyserFlags)
{
    OPERAND_SIZE fileSizeToPass = fileSize;
    int actualizeFileSize = 0;
    int result = 0;
    Diana_PeFile peFile;
    DianaAnalyzeObserverOverMemory analyzeObserver;

    if (!fileSize)
    {
        fileSize = 0x10000;
        actualizeFileSize = 1;
        fileSizeToPass = 0;
    }
    DianaAnalyzeObserverOverMemory_Init(&analyzeObserver,
                                        pPeFile,
                                        fileSize);

    DI_CHECK(DianaPeFile_Init(&peFile, 
                              &analyzeObserver.stream.stream, 
                              fileSizeToPass,
                              DIANA_PE_FILE_FLAGS_MODULE_MODE));
    if (actualizeFileSize)
    {
        result = Diana_ConvertOpSizeToSizeT(&peFile.pImpl->sizeOfModule, 
                                            &analyzeObserver.stream.memoryStream.bufferSize);
        if (result)
        {
            goto cleanup;
        }
    }
    result = Diana_PE_AnalyzePE(&peFile, 
                                &analyzeObserver.parent, 
                                pOwner,
                                analyserFlags);
cleanup:
    DianaPeFile_Free(&peFile);
    return result;
}