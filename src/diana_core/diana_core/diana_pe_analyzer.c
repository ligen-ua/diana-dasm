#include "diana_pe_analyzer.h"

typedef struct Diana_PeAnalyzerCommonParams
{
    Diana_PeFile * pPeFile;
    DianaAnalyzeObserver * pObserver;
    Diana_InstructionsOwner * pOwner;
    void * pPage;
    int pageSize;
    Diana_Stack * pStack;
    DianaAnalyzeParams analyzeParams;
}Diana_PeAnalyzerCommonParams_type;

int DianaAnalyzeObserverOverMemory_ConvertAddressToRelative(void * pThis, 
                                                            OPERAND_SIZE address,
                                                            OPERAND_SIZE * pRelativeOffset,
                                                            int * pbInvalidPointer);

/* a route from an offset of the module. ARMNT: all code is T32, the addresses of functions have bit 0 set */
static
int PeAnalyzer_AnalyzeAt(Diana_PeAnalyzerCommonParams_type * pParams, OPERAND_SIZE offset, int flags)
{
    if (pParams->analyzeParams.arch == DIANA_ARCH_THUMB)
        offset &= ~(OPERAND_SIZE)1;
    return Diana_AnalyzeCodeEx2(pParams->pOwner,
                                pParams->pObserver,
                                pParams->pPeFile->pImpl->dianaMode,
                                offset,
                                pParams->pPeFile->pImpl->sizeOfModule,
                                pParams->pStack,
                                flags,
                                &pParams->analyzeParams);
}

/* ARMNT: 4-byte pointers at 4-aligned offsets; only a pointer with bit 0 set is code (a T32 function),
   an even one points to data */
static
int ScanPageThumb(Diana_PeAnalyzerCommonParams_type * pParams,
                  int pageSize)
{
    const unsigned char * p = (const unsigned char *)pParams->pPage + sizeof(DIANA_SIZE_T);
    const unsigned char * pEnd = (const unsigned char *)pParams->pPage + pageSize - 4 + 1;
    for (; p < pEnd; p += 4)
    {
        DI_UINT32 address = (DI_UINT32)p[0] | ((DI_UINT32)p[1] << 8) | ((DI_UINT32)p[2] << 16) | ((DI_UINT32)p[3] << 24);
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        if (!(address & 1))
            continue;
        DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                       address & ~(DI_UINT32)1,
                                                       DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                       &relativeAddress,
                                                       &result));
        if (result == diaJumpNormal)
            DI_CHECK(PeAnalyzer_AnalyzeAt(pParams, relativeAddress, DI_ANALYSE_BREAK_ON_INVALID_CMD));
    }
    return DI_SUCCESS;
}

// AArch64: 8-byte pointers at 8-aligned offsets; pPage starts with the last pointer of the previous page
static
int ScanPageArm64(Diana_PeAnalyzerCommonParams_type * pParams,
                  int pageSize)
{
    const unsigned char * p = (const unsigned char *)pParams->pPage + sizeof(DIANA_SIZE_T);
    const unsigned char * pEnd = (const unsigned char *)pParams->pPage + pageSize - 8 + 1;
    for (; p < pEnd; p += 8)
    {
        DI_UINT64 address = 0;
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        int i = 0;
        for (i = 7; i >= 0; --i)
            address = (address << 8) | p[i];
        DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                       address,
                                                       DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                       &relativeAddress,
                                                       &result));
        if (result == diaJumpNormal)
        {
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                          pParams->pObserver,
                                          pParams->pPeFile->pImpl->dianaMode,
                                          relativeAddress,
                                          pParams->pPeFile->pImpl->sizeOfModule,
                                          pParams->pStack,
                                          DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                          &pParams->analyzeParams));
        }
    }
    return DI_SUCCESS;
}

static
int ScanPage(Diana_PeAnalyzerCommonParams_type * pParams,
             int pageSize)
{
    char * p = pParams->pPage;
    char * pEnd = p + pageSize - sizeof(DIANA_SIZE_T)+1;
    if (pageSize < sizeof(DIANA_SIZE_T))
        return DI_SUCCESS;
    if (pParams->analyzeParams.arch == DIANA_ARCH_AARCH64)
        return ScanPageArm64(pParams, pageSize);
    if (pParams->analyzeParams.arch == DIANA_ARCH_THUMB)
        return ScanPageThumb(pParams, pageSize);

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
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                        pParams->pObserver,
                                        pParams->pPeFile->pImpl->dianaMode,
                                        relativeAddress,
                                        pParams->pPeFile->pImpl->sizeOfModule,
                                        pParams->pStack,
                                        DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                     &pParams->analyzeParams));
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
    OPERAND_SIZE functionsRaw = 0;
    DIANA_IMAGE_EXPORT_DIRECTORY * pCapturedExportDirectory = (DIANA_IMAGE_EXPORT_DIRECTORY * )pParams->pPage;
    if (pParams->pageSize < sizeof(DIANA_IMAGE_EXPORT_DIRECTORY))
        return DI_INVALID_CONFIGURATION;
        
    exportsRaw = pParams->pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, exportsRaw));
    DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, pCapturedExportDirectory, sizeof(DIANA_IMAGE_EXPORT_DIRECTORY)));
    numberOfFunctions = pCapturedExportDirectory->NumberOfFunctions;
    functionsRaw = pCapturedExportDirectory->AddressOfFunctions;
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
        /* read functions: the table is at AddressOfFunctions, the analysis moves the stream between pages */
        DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream,
                                                        functionsRaw +
                                                        (OPERAND_SIZE)(numberOfFunctions - theRestNumberOfFunctions) * sizeof(DI_UINT32)));
        DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, 
                                pCapturedFunctions, 
                                numberOfFunctionsToRead * sizeof(DI_UINT32)));
        for(i = 0; i < numberOfFunctionsToRead; ++i)
        {
            if (pCapturedFunctions[i] < pParams->pPeFile->pImpl->sizeOfModule)
            {
                DI_CHECK(PeAnalyzer_AnalyzeAt(pParams, pCapturedFunctions[i], 0));
            }
        }
        theRestNumberOfFunctions -= numberOfFunctionsToRead;
    }
    return DI_SUCCESS;
}

#define DIANA_PE_ANALYZER_MAX_TLS_CALLBACKS  256

static DI_UINT64 PeAnalyzer_ReadPointer(const unsigned char * p, int pointerSize)
{
    DI_UINT64 value = 0;
    int i;
    for (i = pointerSize - 1; i >= 0; --i)
        value = (value << 8) | p[i];
    return value;
}

/* A pointer stored in the image as a module offset: the observer knows where a relocated
   image is; an image left at its preferred base has pointers from imageBase */
static int PeAnalyzer_PointerToOffset(Diana_PeAnalyzerCommonParams_type * pParams,
                                      OPERAND_SIZE pointer,
                                      OPERAND_SIZE * pOffset)
{
    Diana_PeFile_impl * pImpl = pParams->pPeFile->pImpl;
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;
    OPERAND_SIZE relativeAddress = 0;

    DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                   pointer,
                                                   DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                   &relativeAddress,
                                                   &result));
    if (result == diaJumpNormal && relativeAddress < pImpl->sizeOfModule)
    {
        *pOffset = relativeAddress;
        return DI_SUCCESS;
    }
    if (pointer >= pImpl->imageBase && pointer - pImpl->imageBase < pImpl->sizeOfModule)
    {
        *pOffset = pointer - pImpl->imageBase;
        return DI_SUCCESS;
    }
    return DI_NOT_FOUND;
}

/* TLS callbacks: the loader calls them before the entry point, often nothing in the code does */
static
int Diana_AnalyzeTlsCallbacks(Diana_PeAnalyzerCommonParams_type * pParams)
{
    Diana_PeFile_impl * pImpl = pParams->pPeFile->pImpl;
    const DIANA_IMAGE_DATA_DIRECTORY * pDirectory = pImpl->pImageDataDirectoryArray + DIANA_IMAGE_DIRECTORY_ENTRY_TLS;
    int pointerSize = pImpl->dianaMode == DIANA_MODE64 ? 8 : 4;
    unsigned char buffer[4 * 8];
    OPERAND_SIZE callbacksOffset = 0;
    int i;

    /* StartAddressOfRawData, EndAddressOfRawData, AddressOfIndex, AddressOfCallBacks */
    if (!pDirectory->VirtualAddress || pDirectory->Size < (DI_UINT32)(4 * pointerSize))
        return DI_SUCCESS;
    if (pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, pDirectory->VirtualAddress) ||
        DianaExactRead(&pParams->pObserver->m_pStream->parent, buffer, 4 * pointerSize))
    {
        return DI_SUCCESS;  /* a broken directory costs only these roots */
    }
    if (PeAnalyzer_PointerToOffset(pParams,
                                   (OPERAND_SIZE)PeAnalyzer_ReadPointer(buffer + 3 * pointerSize, pointerSize),
                                   &callbacksOffset) != DI_SUCCESS)
    {
        return DI_SUCCESS;
    }

    /* the array ends with 0 */
    for (i = 0; i < DIANA_PE_ANALYZER_MAX_TLS_CALLBACKS; ++i)
    {
        OPERAND_SIZE callback = 0;
        OPERAND_SIZE callbackOffset = 0;

        /* the analysis moves the stream */
        if (pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream,
                                                   callbacksOffset + (OPERAND_SIZE)i * pointerSize) ||
            DianaExactRead(&pParams->pObserver->m_pStream->parent, buffer, pointerSize))
        {
            break;
        }
        callback = (OPERAND_SIZE)PeAnalyzer_ReadPointer(buffer, pointerSize);
        if (!callback)
            break;
        if (PeAnalyzer_PointerToOffset(pParams, callback, &callbackOffset) != DI_SUCCESS)
            continue;
        DI_CHECK(PeAnalyzer_AnalyzeAt(pParams, callbackOffset, 0));
    }
    return DI_SUCCESS;
}

// .pdata: RUNTIME_FUNCTION entries, the first field is the start of a function with unwind data.
// x64 entries are {Begin, End, UnwindInfo}, ARM64 and ARMNT entries are {Begin, UnwindData}
static
int Diana_AnalyzePData(Diana_PeAnalyzerCommonParams_type * pParams)
{
    const DIANA_IMAGE_DATA_DIRECTORY * pDirectory =
        pParams->pPeFile->pImpl->pImageDataDirectoryArray + DIANA_IMAGE_DIRECTORY_ENTRY_EXCEPTION;
    DI_UINT32 entrySize = pParams->pPeFile->pImpl->arch == DIANA_ARCH_X86 ? 12 : 8;
    DI_UINT32 theRestCount = 0;
    DI_UINT32 entriesPerPage = (DI_UINT32)pParams->pageSize / entrySize;

    if ((pParams->pPeFile->pImpl->dianaMode != DIANA_MODE64 && pParams->pPeFile->pImpl->arch != DIANA_ARCH_THUMB) ||
        !pDirectory->VirtualAddress)
    {
        return DI_SUCCESS;
    }
    theRestCount = pDirectory->Size / entrySize;
    DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, pDirectory->VirtualAddress));
    while (theRestCount)
    {
        DI_UINT32 i = 0;
        DI_UINT32 count = theRestCount > entriesPerPage ? entriesPerPage : theRestCount;
        const unsigned char * pEntries = (const unsigned char *)pParams->pPage;
        DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, pParams->pPage, count * entrySize));
        theRestCount -= count;
        for (i = 0; i < count; ++i)
        {
            const unsigned char * p = pEntries + i * entrySize;
            OPERAND_SIZE begin = (OPERAND_SIZE)p[0] | ((OPERAND_SIZE)p[1] << 8) | ((OPERAND_SIZE)p[2] << 16) | ((OPERAND_SIZE)p[3] << 24);
            if (!begin || begin >= pParams->pPeFile->pImpl->sizeOfModule)
                continue;
            DI_CHECK(PeAnalyzer_AnalyzeAt(pParams, begin, 0));
        }
        // the analysis moved the stream
        if (theRestCount)
        {
            DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream,
                                                            pDirectory->VirtualAddress + (pDirectory->Size / entrySize - theRestCount) * entrySize));
        }
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
        DI_CHECK(PeAnalyzer_AnalyzeAt(pParams, pParams->pPeFile->pImpl->addressOfEntryPoint, 0));
    }
    DI_CHECK(Diana_AnalyzeTlsCallbacks(pParams));
    // functions with unwind data, roots before the pointer scan below
    DI_CHECK(Diana_AnalyzePData(pParams));
    {
        // scan raw pages
        DI_CHECK(Diana_AnalyzeData(pParams));
    }
    // scan exports
    DI_CHECK(Diana_AnalyzeExports(pParams));
    // AArch64: BTI landing pads nothing else reached
    DI_CHECK(Diana_AnalyzeLandingPads(pParams->pOwner,
                                      pParams->pObserver,
                                      pParams->pPeFile->pImpl->dianaMode,
                                      0,
                                      pParams->pPeFile->pImpl->sizeOfModule,
                                      pParams->pPeFile->pImpl->sizeOfModule,
                                      pParams->pStack,
                                      &pParams->analyzeParams));

    // scan all
    if ((DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags) && pParams->analyzeParams.arch == DIANA_ARCH_THUMB)
    {
        /* every 2-aligned offset, never inside an instruction */
        DI_CHECK(Diana_AnalyzeScanThrough(pParams->pOwner,
                                          pParams->pObserver,
                                          pParams->pPeFile->pImpl->dianaMode,
                                          0,
                                          pParams->pPeFile->pImpl->sizeOfModule,
                                          pParams->pPeFile->pImpl->sizeOfModule,
                                          pParams->pStack,
                                          &pParams->analyzeParams));
    }
    else
    if (DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags)
    {
        OPERAND_SIZE i = 0;
        for(; i < pParams->pPeFile->pImpl->sizeOfModule; i+=4ULL)
        {
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                         pParams->pObserver, 
                                         pParams->pPeFile->pImpl->dianaMode, 
                                         i, 
                                         pParams->pPeFile->pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                     &pParams->analyzeParams));
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
	void * pPage = 0;

    /* x86, AArch64 and ARMNT (no ARM64EC/CHPE) */
    if (pPeFile->pImpl->arch != DIANA_ARCH_X86 && pPeFile->pImpl->arch != DIANA_ARCH_AARCH64 &&
        pPeFile->pImpl->arch != DIANA_ARCH_THUMB)
    {
        return DI_UNSUPPORTED;
    }

    pPage = DIANA_MALLOC(pageSize + sizeof(OPERAND_SIZE));
    DI_CHECK_ALLOC(pPage);

    DI_CHECK_GOTO(Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    stackInited = 1;

    params.pageSize = pageSize;
    params.pPage = pPage;
    params.pPeFile = pPeFile;
    params.pOwner = pOwner;
    params.pObserver = pObserver;
    params.pStack = &stack;
    DianaAnalyzeParams_Init(&params.analyzeParams, pPeFile->pImpl->arch);
    if (pPeFile->pImpl->arch != DIANA_ARCH_X86)
    {
        // the observer knows where the image is, ADRP needs only offset 0 page-aligned
        params.analyzeParams.flags = DIANA_ANALYZE_ADDRESSES_RELATIVE;
    }

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