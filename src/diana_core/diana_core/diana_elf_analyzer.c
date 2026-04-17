#include "diana_elf_analyzer.h"

typedef struct Diana_ElfAnalyzerSym64_
{
    DI_UINT32 st_name;
    DI_UINT8  st_info;
    DI_UINT8  st_other;
    DI_UINT16 st_shndx;
    DI_UINT64 st_value;
    DI_UINT64 st_size;
} Diana_ElfAnalyzerSym64;

typedef struct Diana_ElfAnalyzerCommonParams_
{
    Diana_ElfFile           * pElfFile;
    DianaAnalyzeObserver    * pObserver;
    Diana_InstructionsOwner * pOwner;
    void                    * pPage;
    int                       pageSize;
    Diana_Stack             * pStack;
} Diana_ElfAnalyzerCommonParams_type;

static DI_UINT16 ElfAnalyzer_rd16(const unsigned char * p, int isLE)
{
    if (isLE)
        return (DI_UINT16)p[0] | ((DI_UINT16)p[1] << 8);
    return (DI_UINT16)p[1] | ((DI_UINT16)p[0] << 8);
}

static DI_UINT32 ElfAnalyzer_rd32(const unsigned char * p, int isLE)
{
    if (isLE)
        return (DI_UINT32)p[0] | ((DI_UINT32)p[1] << 8) | ((DI_UINT32)p[2] << 16) | ((DI_UINT32)p[3] << 24);
    return (DI_UINT32)p[3] | ((DI_UINT32)p[2] << 8) | ((DI_UINT32)p[1] << 16) | ((DI_UINT32)p[0] << 24);
}

static DI_UINT64 ElfAnalyzer_rd64(const unsigned char * p, int isLE)
{
    if (isLE)
        return (DI_UINT64)p[0]       | ((DI_UINT64)p[1] << 8)  | ((DI_UINT64)p[2] << 16) | ((DI_UINT64)p[3] << 24)
             | ((DI_UINT64)p[4] << 32) | ((DI_UINT64)p[5] << 40) | ((DI_UINT64)p[6] << 48) | ((DI_UINT64)p[7] << 56);
    return (DI_UINT64)p[7]       | ((DI_UINT64)p[6] << 8)  | ((DI_UINT64)p[5] << 16) | ((DI_UINT64)p[4] << 24)
         | ((DI_UINT64)p[3] << 32) | ((DI_UINT64)p[2] << 40) | ((DI_UINT64)p[1] << 48) | ((DI_UINT64)p[0] << 56);
}

static int ElfAnalyzer_ReadSym(DianaAnalyzeObserver * pObserver,
                                OPERAND_SIZE symEntryAddr,
                                int is64bit,
                                int isLE,
                                Diana_ElfAnalyzerSym64 * pSym)
{
    if (is64bit)
    {
        unsigned char buf[24];
        DI_CHECK(pObserver->m_pStream->pMoveTo(pObserver->m_pStream, symEntryAddr));
        DI_CHECK(DianaExactRead(&pObserver->m_pStream->parent, buf, sizeof(buf)));
        pSym->st_name  = ElfAnalyzer_rd32(buf + 0x00, isLE);
        pSym->st_info  = buf[0x04];
        pSym->st_other = buf[0x05];
        pSym->st_shndx = ElfAnalyzer_rd16(buf + 0x06, isLE);
        pSym->st_value = ElfAnalyzer_rd64(buf + 0x08, isLE);
        pSym->st_size  = ElfAnalyzer_rd64(buf + 0x10, isLE);
    }
    else
    {
        unsigned char buf[16];
        DI_CHECK(pObserver->m_pStream->pMoveTo(pObserver->m_pStream, symEntryAddr));
        DI_CHECK(DianaExactRead(&pObserver->m_pStream->parent, buf, sizeof(buf)));
        pSym->st_name  = ElfAnalyzer_rd32(buf + 0x00, isLE);
        pSym->st_value = (DI_UINT64)ElfAnalyzer_rd32(buf + 0x04, isLE);
        pSym->st_size  = (DI_UINT64)ElfAnalyzer_rd32(buf + 0x08, isLE);
        pSym->st_info  = buf[0x0C];
        pSym->st_other = buf[0x0D];
        pSym->st_shndx = ElfAnalyzer_rd16(buf + 0x0E, isLE);
    }
    return DI_SUCCESS;
}

static int Diana_ELF_AnalyzeSymbols(Diana_ElfAnalyzerCommonParams_type * pParams)
{
    Diana_ElfFile_impl * pImpl = pParams->pElfFile->pImpl;
    const int is64bit = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
    const int isLE    = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;
    OPERAND_SIZE symtabAddr = 0;
    DI_UINT64    symEntSize = is64bit ? 24 : 16;

    if (!pImpl->dynamicAddress || !pImpl->dynamicSize)
        return DI_SUCCESS;

    /* Walk dynamic section to find DT_SYMTAB and DT_SYMENT */
    {
        OPERAND_SIZE dynAddr = pImpl->dynamicAddress;
        OPERAND_SIZE dynEnd  = dynAddr + (OPERAND_SIZE)pImpl->dynamicSize;
        OPERAND_SIZE entSize = is64bit ? 16 : 8;

        for (; dynAddr < dynEnd; dynAddr += entSize)
        {
            unsigned char buf[16];
            DI_INT64  d_tag;
            DI_UINT64 d_val;

            DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream, dynAddr));
            DI_CHECK(DianaExactRead(&pParams->pObserver->m_pStream->parent, buf, (int)entSize));

            if (is64bit)
            {
                d_tag = (DI_INT64)ElfAnalyzer_rd64(buf + 0, isLE);
                d_val = ElfAnalyzer_rd64(buf + 8, isLE);
            }
            else
            {
                d_tag = (DI_INT64)(DI_INT32)ElfAnalyzer_rd32(buf + 0, isLE);
                d_val = (DI_UINT64)ElfAnalyzer_rd32(buf + 4, isLE);
            }

            if (d_tag == 0 /* DT_NULL */) break;
            if (d_tag == 6 /* DT_SYMTAB */) symtabAddr = (OPERAND_SIZE)d_val;
            if (d_tag == 11 /* DT_SYMENT */) symEntSize = d_val;
        }
    }

    if (!symtabAddr || !symEntSize)
        return DI_SUCCESS;

    /* Iterate symbols; no explicit count in MODULE_MODE so cap at 65536 */
    {
        OPERAND_SIZE symAddr = symtabAddr;
        DI_UINT64 i;
        for (i = 0; i < 65536; ++i, symAddr += symEntSize)
        {
            Diana_ElfAnalyzerSym64 sym;
            int symType;

            if (ElfAnalyzer_ReadSym(pParams->pObserver, symAddr, is64bit, isLE, &sym))
                break;

            if (sym.st_value == 0 && sym.st_shndx == 0 && sym.st_info == 0)
                continue;

            symType = DIANA_ELF_ST_TYPE(sym.st_info);
            if (symType != DIANA_STT_FUNC)
                continue;
            if (sym.st_shndx == DIANA_STN_UNDEF)
                continue;
            if (sym.st_value == 0)
                continue;
            if (sym.st_value >= pImpl->sizeOfModule)
                break;

            DI_CHECK(Diana_AnalyzeCode(pParams->pOwner,
                                       pParams->pObserver,
                                       pParams->pElfFile->dianaMode,
                                       (OPERAND_SIZE)sym.st_value,
                                       pImpl->sizeOfModule));
        }
    }
    return DI_SUCCESS;
}

static int ELF_ScanPage(Diana_ElfAnalyzerCommonParams_type * pParams, int pageSize)
{
    char * p = pParams->pPage;
    char * pEnd = p + pageSize - sizeof(DIANA_SIZE_T) + 1;
    if (pageSize < (int)sizeof(DIANA_SIZE_T))
        return DI_SUCCESS;

    for (++p; p < pEnd; ++p)
    {
        DIANA_SIZE_T * pAddress = (DIANA_SIZE_T *)p;
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        if (pParams->pElfFile->dianaMode == DIANA_MODE32)
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
                                         pParams->pElfFile->dianaMode,
                                         relativeAddress,
                                         pParams->pElfFile->pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD));
        }
    }
    return DI_SUCCESS;
}

static int Diana_ELF_AnalyzeData(Diana_ElfAnalyzerCommonParams_type * pParams)
{
    Diana_ElfFile_impl * pImpl = pParams->pElfFile->pImpl;
    int i;
    for (i = 0; i < pImpl->capturedSegmentCount; ++i)
    {
        DIANA_ELF_PROGRAM_HEADER * pSeg = pImpl->pCapturedSegments + i;
        OPERAND_SIZE allReadBytes = 0;
        DIANA_SIZE_T pageLastPointer = 0;

        if (pSeg->p_type != DIANA_PT_LOAD)
            continue;

        DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream,
                                                        pSeg->p_vaddr));
        for (;;)
        {
            OPERAND_SIZE sizeToRead = pParams->pageSize;
            int readBytes = 0;
            int result;

            if (allReadBytes >= pSeg->p_memsz)
                break;
            if (pSeg->p_memsz - allReadBytes < sizeToRead)
            {
                sizeToRead = pSeg->p_memsz - allReadBytes;
                if (sizeToRead == 0) break;
            }

            result = pParams->pObserver->m_pStream->parent.pReadFnc(
                         pParams->pObserver->m_pStream,
                         (char*)pParams->pPage + sizeof(DIANA_SIZE_T),
                         (int)sizeToRead,
                         &readBytes);
            if (result || !readBytes)
                break;

            allReadBytes += readBytes;
            *(DIANA_SIZE_T*)pParams->pPage = pageLastPointer;
            DI_CHECK(ELF_ScanPage(pParams, readBytes + (int)sizeof(DIANA_SIZE_T)));
            if (readBytes >= (int)sizeof(DIANA_SIZE_T))
                pageLastPointer = *(DIANA_SIZE_T*)((char*)pParams->pPage + readBytes);

            if (allReadBytes >= pSeg->p_memsz)
                break;
        }
    }
    return DI_SUCCESS;
}

static int Diana_ELF_AnalyzeELFImplUsingBuffer(Diana_ElfAnalyzerCommonParams_type * pParams,
                                                int analyserFlags)
{
    Diana_ElfFile_impl * pImpl = pParams->pElfFile->pImpl;

    if (pImpl->elfHeader.e_entry && pImpl->elfHeader.e_entry < pImpl->sizeOfModule)
    {
        DI_CHECK(Diana_AnalyzeCodeEx(pParams->pOwner,
                                     pParams->pObserver,
                                     pParams->pElfFile->dianaMode,
                                     (OPERAND_SIZE)pImpl->elfHeader.e_entry,
                                     pImpl->sizeOfModule,
                                     pParams->pStack,
                                     0));
    }

    DI_CHECK(Diana_ELF_AnalyzeSymbols(pParams));
    DI_CHECK(Diana_ELF_AnalyzeData(pParams));

    if (DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags)
    {
        OPERAND_SIZE i = 0;
        for (; i < pImpl->sizeOfModule; i += 4ULL)
        {
            DI_CHECK(Diana_AnalyzeCodeEx(pParams->pOwner,
                                         pParams->pObserver,
                                         pParams->pElfFile->dianaMode,
                                         i,
                                         pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD));
        }
    }
    return DI_SUCCESS;
}

static int Diana_ELF_AnalyzeELFImpl(Diana_ElfFile * pElfFile,
                                     DianaAnalyzeObserver * pObserver,
                                     Diana_InstructionsOwner * pOwner,
                                     int analyserFlags)
{
    Diana_Stack stack;
    int pageSize = 0x10000;
    int status = 0;
    int stackInited = 0;
    Diana_ElfAnalyzerCommonParams_type params;

    void * pPage = DIANA_MALLOC(pageSize + sizeof(OPERAND_SIZE));
    DI_CHECK_ALLOC(pPage);

    DI_CHECK_GOTO(Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    stackInited = 1;

    params.pageSize  = pageSize;
    params.pPage     = pPage;
    params.pElfFile  = pElfFile;
    params.pOwner    = pOwner;
    params.pObserver = pObserver;
    params.pStack    = &stack;

    status = Diana_ELF_AnalyzeELFImplUsingBuffer(&params, analyserFlags);
cleanup:
    if (stackInited)
        Diana_Stack_Free(&stack);
    DIANA_FREE(pPage);
    return status;
}

int Diana_ELF_AnalyzeELF(Diana_ElfFile * pElfFile,
                          DianaAnalyzeObserver * pObserver,
                          Diana_InstructionsOwner * pOwner,
                          int analyserFlags)
{
    int status = 0;
    DI_CHECK(Diana_InstructionsOwner_Init(pOwner,
                                          pElfFile->pImpl->sizeOfModule,
                                          0x10000));
    status = Diana_ELF_AnalyzeELFImpl(pElfFile, pObserver, pOwner, analyserFlags);
    if (status)
        Diana_InstructionsOwner_Free(pOwner);
    return status;
}
