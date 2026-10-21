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
    DianaAnalyzeParams analyzeParams;
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

static int IsElfArm32(const Diana_ElfAnalyzerCommonParams_type * pParams)
{
    return pParams->pElfFile->arch == DIANA_ARCH_ARM32 || pParams->pElfFile->arch == DIANA_ARCH_THUMB;
}

/* a route from an offset of the module. AArch32: bit 0 of the address (a symbol, the entry, a pointer)
   selects T32, as BX and the loader see it */
static int ElfAnalyzer_AnalyzeAt(Diana_ElfAnalyzerCommonParams_type * pParams, OPERAND_SIZE offset, int flags)
{
    DianaAnalyzeParams analyzeParams = pParams->analyzeParams;
    if (IsElfArm32(pParams))
    {
        analyzeParams.arch = (offset & 1) ? DIANA_ARCH_THUMB : DIANA_ARCH_ARM32;
        offset &= ~(OPERAND_SIZE)1;
    }
    return Diana_AnalyzeCodeEx2(pParams->pOwner,
                                pParams->pObserver,
                                pParams->pElfFile->dianaMode,
                                offset,
                                pParams->pElfFile->pImpl->sizeOfModule,
                                pParams->pStack,
                                flags,
                                &analyzeParams);
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

/* A pointer the loader relocated (most dynamic tags, the init arrays) is an absolute address in the module;
   others are link-time addresses */
static int ElfAnalyzer_ImagePointerToOffset(Diana_ElfAnalyzerCommonParams_type * pParams,
                                            OPERAND_SIZE pointer,
                                            OPERAND_SIZE * pOffset)
{
    Diana_ElfFile_impl * pImpl = pParams->pElfFile->pImpl;
    OPERAND_SIZE relativeAddress = 0;
    DianaAnalyzeAddressResult_type result = diaJumpInvalid;

    *pOffset = pointer;
    /* ET_EXEC: the loader does not move it, every pointer is a link-time address */
    if (pImpl->elfHeader.e_type == DIANA_ET_EXEC &&
        DianaElfFile_LinkToOffset(pImpl, pointer, pOffset) == DI_SUCCESS)
    {
        return DI_SUCCESS;
    }
    DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                   pointer,
                                                   DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                   &relativeAddress,
                                                   &result));
    if (result == diaJumpNormal)
        *pOffset = relativeAddress;
    else
        DianaElfFile_LinkToOffset(pImpl, pointer, pOffset);
    return DI_SUCCESS;
}

static int Diana_ELF_AnalyzeSymbols(Diana_ElfAnalyzerCommonParams_type * pParams)
{
    Diana_ElfFile_impl * pImpl = pParams->pElfFile->pImpl;
    const int is64bit = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
    const int isLE    = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;
    OPERAND_SIZE symtabAddr = 0;
    DI_UINT64    symEntSize = is64bit ? 24 : 16;

    OPERAND_SIZE dynAddr = 0;

    if (!pImpl->dynamicAddress || !pImpl->dynamicSize)
        return DI_SUCCESS;
    if (DianaElfFile_LinkToOffset(pImpl, pImpl->dynamicAddress, &dynAddr))
        return DI_SUCCESS;

    /* Walk dynamic section to find DT_SYMTAB and DT_SYMENT */
    {
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
    /* relocated in a mapped image, a link-time address otherwise */
    DI_CHECK(ElfAnalyzer_ImagePointerToOffset(pParams, symtabAddr, &symtabAddr));
    if (symtabAddr >= pImpl->sizeOfModule)
        return DI_SUCCESS;

    /* Iterate symbols; no explicit count in MODULE_MODE so cap at 65536 */
    {
        OPERAND_SIZE symAddr = symtabAddr;
        DI_UINT64 i;
        for (i = 0; i < 65536; ++i, symAddr += symEntSize)
        {
            Diana_ElfAnalyzerSym64 sym;
            int symType;
            OPERAND_SIZE symOffset = 0;

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
            if (DianaElfFile_LinkToOffset(pImpl, (OPERAND_SIZE)sym.st_value, &symOffset))
                break;

            DI_CHECK(ElfAnalyzer_AnalyzeAt(pParams, symOffset, 0));
        }
    }
    return DI_SUCCESS;
}

typedef struct Diana_ElfAnalyzerStartupContext_
{
    Diana_ElfAnalyzerCommonParams_type * pParams;
    int analyzeStatus;
} Diana_ElfAnalyzerStartupContext;

/* A pointer the loader relocated is an absolute address in the module; others are link-time addresses */
static int ElfAnalyzer_PointerToOffset(void * pContext, OPERAND_SIZE pointer, OPERAND_SIZE * pOffset)
{
    Diana_ElfAnalyzerStartupContext * pStartup = (Diana_ElfAnalyzerStartupContext *)pContext;
    return ElfAnalyzer_ImagePointerToOffset(pStartup->pParams, pointer, pOffset);
}

static int ElfAnalyzer_OnStartupRoutine(void * pContext, const DianaElfStartupRoutine * pRoutine)
{
    Diana_ElfAnalyzerStartupContext * pStartup = (Diana_ElfAnalyzerStartupContext *)pContext;
    Diana_ElfAnalyzerCommonParams_type * pParams = pStartup->pParams;
    OPERAND_SIZE sizeOfModule = pParams->pElfFile->pImpl->sizeOfModule;

    if (!pRoutine->offset || pRoutine->offset >= sizeOfModule)
        return DI_SUCCESS;
    pStartup->analyzeStatus = ElfAnalyzer_AnalyzeAt(pParams, pRoutine->offset, 0);
    return pStartup->analyzeStatus;
}

/* IFUNC resolvers and initializers: the loader runs them before main, often nothing calls them */
static int Diana_ELF_AnalyzeStartupRoutines(Diana_ElfAnalyzerCommonParams_type * pParams)
{
    Diana_ElfAnalyzerStartupContext startup;
    startup.pParams = pParams;
    startup.analyzeStatus = DI_SUCCESS;

    /* a malformed dynamic section only costs these roots; a failed analysis fails it all */
    DianaElfFile_QueryStartupRoutines(pParams->pElfFile,
                                      pParams->pObserver->m_pStream,
                                      0,
                                      ElfAnalyzer_PointerToOffset,
                                      &startup,
                                      ElfAnalyzer_OnStartupRoutine,
                                      &startup,
                                      0);
    return startup.analyzeStatus;
}

// AArch64: 8-byte pointers at 8-aligned offsets; pPage starts with the last pointer of the previous page
static int ELF_ScanPageArm64(Diana_ElfAnalyzerCommonParams_type * pParams, int pageSize)
{
    const int isLE = pParams->pElfFile->pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;
    const unsigned char * p = (const unsigned char *)pParams->pPage + sizeof(DIANA_SIZE_T);
    const unsigned char * pEnd = (const unsigned char *)pParams->pPage + pageSize - 8 + 1;
    for (; p < pEnd; p += 8)
    {
        DI_UINT64 address = ElfAnalyzer_rd64(p, isLE);
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                       address,
                                                       DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                       &relativeAddress,
                                                       &result));
        if (result == diaJumpNormal)
        {
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                          pParams->pObserver,
                                          pParams->pElfFile->dianaMode,
                                          relativeAddress,
                                          pParams->pElfFile->pImpl->sizeOfModule,
                                          pParams->pStack,
                                          DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                          &pParams->analyzeParams));
        }
    }
    return DI_SUCCESS;
}

/* AArch32: 4-byte pointers at 4-aligned offsets. Bit 0 set is T32 code; an even pointer is A32 code only
   in a module that is A32 by default, in T32 code data is what even pointers point to */
static int ELF_ScanPageArm32(Diana_ElfAnalyzerCommonParams_type * pParams, int pageSize)
{
    const int isLE = pParams->pElfFile->pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;
    const unsigned char * p = (const unsigned char *)pParams->pPage + sizeof(DIANA_SIZE_T);
    const unsigned char * pEnd = (const unsigned char *)pParams->pPage + pageSize - 4 + 1;
    for (; p < pEnd; p += 4)
    {
        DI_UINT32 address = ElfAnalyzer_rd32(p, isLE);
        OPERAND_SIZE relativeAddress = 0;
        DianaAnalyzeAddressResult_type result = diaJumpInvalid;
        if (!(address & 1) && pParams->pElfFile->arch == DIANA_ARCH_THUMB)
            continue;
        DI_CHECK(pParams->pObserver->m_pAnalyzeAddress(pParams->pObserver,
                                                       address,
                                                       DIANA_ANALYZE_ABSOLUTE_ADDRESS,
                                                       &relativeAddress,
                                                       &result));
        if (result == diaJumpNormal)
            DI_CHECK(ElfAnalyzer_AnalyzeAt(pParams, relativeAddress, DI_ANALYSE_BREAK_ON_INVALID_CMD));
    }
    return DI_SUCCESS;
}

static int ELF_ScanPage(Diana_ElfAnalyzerCommonParams_type * pParams, int pageSize)
{
    char * p = pParams->pPage;
    char * pEnd = p + pageSize - sizeof(DIANA_SIZE_T) + 1;
    if (pageSize < (int)sizeof(DIANA_SIZE_T))
        return DI_SUCCESS;
    if (pParams->analyzeParams.arch == DIANA_ARCH_AARCH64)
        return ELF_ScanPageArm64(pParams, pageSize);
    if (IsElfArm32(pParams))
        return ELF_ScanPageArm32(pParams, pageSize);

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
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                         pParams->pObserver,
                                         pParams->pElfFile->dianaMode,
                                         relativeAddress,
                                         pParams->pElfFile->pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                     &pParams->analyzeParams));
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
        OPERAND_SIZE segOffset = 0;
        DIANA_SIZE_T pageLastPointer = 0;

        if (pSeg->p_type != DIANA_PT_LOAD)
            continue;

        if (DianaElfFile_LinkToOffset(pImpl, (OPERAND_SIZE)pSeg->p_vaddr, &segOffset))
            continue;
        DI_CHECK(pParams->pObserver->m_pStream->pMoveTo(pParams->pObserver->m_pStream,
                                                        segOffset));
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
    OPERAND_SIZE entry = 0;

    if (pImpl->elfHeader.e_entry &&
        DianaElfFile_LinkToOffset(pImpl, (OPERAND_SIZE)pImpl->elfHeader.e_entry, &entry) == DI_SUCCESS)
    {
        DI_CHECK(ElfAnalyzer_AnalyzeAt(pParams, entry, 0));
    }

    DI_CHECK(Diana_ELF_AnalyzeSymbols(pParams));
    DI_CHECK(Diana_ELF_AnalyzeStartupRoutines(pParams));
    DI_CHECK(Diana_ELF_AnalyzeData(pParams));
    // AArch64: BTI landing pads nothing else reached (C++ exception landing pads)
    DI_CHECK(Diana_AnalyzeLandingPads(pParams->pOwner,
                                      pParams->pObserver,
                                      pParams->pElfFile->dianaMode,
                                      0,
                                      pImpl->sizeOfModule,
                                      pImpl->sizeOfModule,
                                      pParams->pStack,
                                      &pParams->analyzeParams));

    if ((DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags) && IsElfArm32(pParams))
    {
        /* every aligned offset, in the module's own mode, never inside an instruction */
        DI_CHECK(Diana_AnalyzeScanThrough(pParams->pOwner,
                                          pParams->pObserver,
                                          pParams->pElfFile->dianaMode,
                                          0,
                                          pImpl->sizeOfModule,
                                          pImpl->sizeOfModule,
                                          pParams->pStack,
                                          &pParams->analyzeParams));
    }
    else
    if (DI_ANALYSE_PE_FILE_SCAN_THROUGH & analyserFlags)
    {
        OPERAND_SIZE i = 0;
        for (; i < pImpl->sizeOfModule; i += 4ULL)
        {
            DI_CHECK(Diana_AnalyzeCodeEx2(pParams->pOwner,
                                         pParams->pObserver,
                                         pParams->pElfFile->dianaMode,
                                         i,
                                         pImpl->sizeOfModule,
                                         pParams->pStack,
                                         DI_ANALYSE_BREAK_ON_INVALID_CMD,
                                     &pParams->analyzeParams));
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
    void * pPage = 0;

    /* x86, AArch64 and AArch32 */
    if (pElfFile->arch != DIANA_ARCH_X86 && pElfFile->arch != DIANA_ARCH_AARCH64 &&
        pElfFile->arch != DIANA_ARCH_ARM32 && pElfFile->arch != DIANA_ARCH_THUMB)
    {
        return DI_UNSUPPORTED;
    }

    pPage = DIANA_MALLOC(pageSize + sizeof(OPERAND_SIZE));
    DI_CHECK_ALLOC(pPage);

    DI_CHECK_GOTO(Diana_Stack_Init(&stack, 4096, sizeof(Diana_RouteInfo)));
    stackInited = 1;

    params.pageSize  = pageSize;
    params.pPage     = pPage;
    params.pElfFile  = pElfFile;
    params.pOwner    = pOwner;
    params.pObserver = pObserver;
    params.pStack    = &stack;
    DianaAnalyzeParams_Init(&params.analyzeParams, pElfFile->arch);
    if (pElfFile->arch != DIANA_ARCH_X86)
    {
        // the observer knows where the image is, ADRP needs only offset 0 page-aligned
        params.analyzeParams.flags = DIANA_ANALYZE_ADDRESSES_RELATIVE;
    }
    if (pElfFile->arch == DIANA_ARCH_ARM32 || pElfFile->arch == DIANA_ARCH_THUMB)
    {
        /* AArch32: the module's own mode, T32 for most armhf code */
        Diana_ElfAnalyzerStartupContext pointers;
        int arch = pElfFile->arch;
        pointers.pParams = &params;
        pointers.analyzeStatus = DI_SUCCESS;
        if (DianaElfFile_QueryArmCodeArch(pElfFile, pObserver->m_pStream, 0, ElfAnalyzer_PointerToOffset, &pointers,
                                          0, &arch) == DI_SUCCESS)
        {
            pElfFile->arch = arch;
            params.analyzeParams.arch = arch;
        }
    }

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
