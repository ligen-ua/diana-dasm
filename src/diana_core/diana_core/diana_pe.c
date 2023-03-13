#include "diana_pe.h"
#include "diana_streams.h"

static
void Diana_PeFile_impl_CommonInit(Diana_PeFile_impl  * pImpl,
                                  int mode,
                                  OPERAND_SIZE sizeOfFile,
                                  OPERAND_SIZE sizeOfImpl,
                                  int optionalHeaderSize)
{
    pImpl->dianaMode = mode;
    pImpl->sizeOfInitialFile = sizeOfFile;
    pImpl->sizeOfImpl = sizeOfImpl;
    pImpl->optionalHeaderSize = optionalHeaderSize;
}
static
void Diana_InitCustomFields(Diana_PeFile_impl * pImpl,
                            DI_UINT32  addressOfEntryPoint,
                            DIANA_IMAGE_DATA_DIRECTORY * pImageDataDirectoryArray,
                            DI_UINT32  sizeOfHeaders,
                            OPERAND_SIZE imageBase)
{
    pImpl->addressOfEntryPoint = addressOfEntryPoint;
    pImpl->pImageDataDirectoryArray = pImageDataDirectoryArray;
    pImpl->sizeOfHeaders = sizeOfHeaders;
    pImpl->imageBase = imageBase;
}

static 
int DI_Init386(Diana_PeFile_impl * pImpl_in,
               DianaReadStream * pStream)
{
    Diana_PeFile32_impl * pImpl = (Diana_PeFile32_impl * )pImpl_in;
    DIANA_IMAGE_OPTIONAL_HEADER32 * pOpt =  (DIANA_IMAGE_OPTIONAL_HEADER32 * )(pImpl + 1);

    DI_CHECK(DianaExactRead(pStream, pOpt, sizeof(DIANA_IMAGE_OPTIONAL_HEADER32)));
    pImpl->pOptionalHeader = pOpt;
    Diana_InitCustomFields(pImpl_in, 
                           pOpt->AddressOfEntryPoint, 
                           pOpt->DataDirectory,
                           pOpt->SizeOfHeaders,
                           pOpt->ImageBase);
    return DI_SUCCESS;
}
static 
int DI_InitAmd64(Diana_PeFile_impl * pImpl_in,
                 DianaReadStream * pStream)
{
    Diana_PeFile64_impl * pImpl = (Diana_PeFile64_impl * )pImpl_in;
    DIANA_IMAGE_OPTIONAL_HEADER64 * pOpt =  (DIANA_IMAGE_OPTIONAL_HEADER64 * )(pImpl + 1);

    DI_CHECK(DianaExactRead(pStream, pOpt, sizeof(DIANA_IMAGE_OPTIONAL_HEADER64)));
    pImpl->pOptionalHeader = pOpt;
    Diana_InitCustomFields(pImpl_in, 
                           pOpt->AddressOfEntryPoint, 
                           pOpt->DataDirectory,
                           pOpt->SizeOfHeaders,
                           pOpt->ImageBase);
    return DI_SUCCESS;
}

static
int Diana_VerifyDosHeader(DIANA_IMAGE_DOS_HEADER * pDosHeader,
                         OPERAND_SIZE sizeOfFile)
{
    if (sizeOfFile && sizeOfFile <= sizeof(DIANA_IMAGE_DOS_HEADER))
        return DI_INVALID_INPUT;

    if (pDosHeader->e_magic[0] != 'M' ||
        pDosHeader->e_magic[1] != 'Z')
    {
        return DI_INVALID_INPUT;
    }

    if (sizeOfFile)
    {
        if (pDosHeader->e_lfanew >= sizeOfFile)
        {
            return DI_INVALID_INPUT;
        }

        if (sizeof(DIANA_IMAGE_NT_HEADERS) >= sizeOfFile - pDosHeader->e_lfanew)
        {
            return DI_INVALID_INPUT;
        }   
    }
    return DI_SUCCESS;
}

static
int ReadOptionalHeader(Diana_PeFile_impl * pImpl,
                       DianaReadStream * pStream,
                       int * pDisasmMode,
                       int * pOptionalHeaderSize)
{
    if (DIANA_STRNCMP(pImpl->ntHeaders.Signature, "PE", 3))
    {
        return DI_INVALID_INPUT;
    }
    switch( pImpl->ntHeaders.FileHeader.Machine )
    {
    case DIANA_IMAGE_FILE_MACHINE_I386:
        *pDisasmMode = DIANA_MODE32;
        *pOptionalHeaderSize = sizeof(DIANA_IMAGE_OPTIONAL_HEADER32);
        return DI_Init386(pImpl, pStream);

    case DIANA_IMAGE_FILE_MACHINE_AMD64:
        *pDisasmMode = DIANA_MODE64;
        *pOptionalHeaderSize = sizeof(DIANA_IMAGE_OPTIONAL_HEADER64);
        return DI_InitAmd64(pImpl, pStream);
    }
    return DI_UNSUPPORTED_COMMAND;
}

static
DIANA_SIZE_T Diana_GetMaxSize(DIANA_SIZE_T size1, DIANA_SIZE_T size2)
{
    if (size1 > size2)
        return size1;
    return size2;
}

static
int Diana_InitPeFileImpl(Diana_PeFile * pPeFile,
                         DianaMovableReadStream * pStream,
                         OPERAND_SIZE sizeOfFile,
                         int flags)
{
    int mode = 0;
    int sizeOfImpl = 0;
    int optionalHeaderSize = 0;
    Diana_PeFile_impl * pImpl= 0;
    DIANA_IMAGE_SECTION_HEADER * pSectionHeader = 0;
    
    DIANA_MEMSET(pPeFile, 0, sizeof(*pPeFile));
    pPeFile->flags = flags;
    // allocate impl header
    sizeOfImpl = (int)Diana_GetMaxSize(sizeof(Diana_PeFile64_impl), sizeof(Diana_PeFile32_impl)) + 
                 (int)Diana_GetMaxSize(sizeof(DIANA_IMAGE_OPTIONAL_HEADER64), sizeof(DIANA_IMAGE_OPTIONAL_HEADER32));
    pImpl = DIANA_MALLOC(sizeOfImpl);
    DI_CHECK_ALLOC(pImpl);
    pPeFile->pImpl = pImpl;
    DIANA_MEMSET(pImpl, 0, sizeof(*pImpl));

    // read dos header
    DI_CHECK(pStream->pMoveTo(pStream, 0));
    DI_CHECK(DianaExactRead(&pStream->parent, &pImpl->dosHeader, sizeof(DIANA_IMAGE_DOS_HEADER)));
    DI_CHECK(Diana_VerifyDosHeader(&pImpl->dosHeader, sizeOfFile));

    // read nt header
    DI_CHECK(pStream->pMoveTo(pStream, pImpl->dosHeader.e_lfanew));
    DI_CHECK(DianaExactRead(&pStream->parent, &pImpl->ntHeaders, sizeof(DIANA_IMAGE_NT_HEADERS)));

    // read optional header
    DI_CHECK(ReadOptionalHeader(pImpl, &pStream->parent, &mode, &optionalHeaderSize));
    Diana_PeFile_impl_CommonInit(pImpl, mode, sizeOfFile, sizeOfImpl, optionalHeaderSize);

    // init sections
    {
        int sectionOffset = pImpl->dosHeader.e_lfanew + sizeof(DIANA_IMAGE_NT_HEADERS) + optionalHeaderSize;
        DI_CHECK(pStream->pMoveTo(pStream, sectionOffset));
    
        pSectionHeader = DIANA_MALLOC(pImpl->ntHeaders.FileHeader.NumberOfSections * sizeof(DIANA_IMAGE_SECTION_HEADER));
        DI_CHECK_ALLOC(pSectionHeader);
        DIANA_MEMSET(pSectionHeader, 0, pImpl->ntHeaders.FileHeader.NumberOfSections * sizeof(DIANA_IMAGE_SECTION_HEADER));
        pImpl->pCapturedSections = pSectionHeader;

        DI_CHECK(DianaExactRead(&pStream->parent, pSectionHeader, pImpl->ntHeaders.FileHeader.NumberOfSections * sizeof(DIANA_IMAGE_SECTION_HEADER)));
        pImpl->capturedSectionCount = pImpl->ntHeaders.FileHeader.NumberOfSections;
    }
    // init module size
    {
        // calculate it
        OPERAND_SIZE size = 0;
        int i;
        for(i = 0; i < pImpl->capturedSectionCount; ++i)
        {
            OPERAND_SIZE currentSectionMax = 0;
            DIANA_IMAGE_SECTION_HEADER * pHeader = pImpl->pCapturedSections + i;
            currentSectionMax = pHeader->VirtualAddress + pHeader->Misc.VirtualSize;
            if (currentSectionMax > size)
            {
                size = currentSectionMax;
            }
        }
        pImpl->sizeOfModule = size;
    }
    return DI_SUCCESS;
}

int DianaPeFile_Init(Diana_PeFile * pPeFile,
                     DianaMovableReadStream * pStream,
                     OPERAND_SIZE sizeOfFile,
                     int flags)
{
    int status = Diana_InitPeFileImpl(pPeFile, 
                                      pStream, 
                                      sizeOfFile,
                                      flags);
    if (status)
    {
        DianaPeFile_Free(pPeFile);
    }
    return status;
}

void DianaPeFile_Free(Diana_PeFile * pPeFile)
{
    if (pPeFile->pImpl)
    {
        if (pPeFile->pImpl->pCapturedSections)
        {
            DIANA_FREE(pPeFile->pImpl->pCapturedSections);
            pPeFile->pImpl->pCapturedSections = 0;
        }
        DIANA_FREE(pPeFile->pImpl);
        pPeFile->pImpl = 0;
    }
}



#define DIANA_IMAGE_REL_BASED_ABSOLUTE              0
#define DIANA_IMAGE_REL_BASED_HIGH                  1
#define DIANA_IMAGE_REL_BASED_LOW                   2
#define DIANA_IMAGE_REL_BASED_HIGHLOW               3
#define DIANA_IMAGE_REL_BASED_HIGHADJ               4
#define DIANA_IMAGE_REL_BASED_MIPS_JMPADDR          5
#define DIANA_IMAGE_REL_BASED_SECTION               6
#define DIANA_IMAGE_REL_BASED_MIPS_JMPADDR16        9
#define DIANA_IMAGE_REL_BASED_IA64_IMM64            9
#define DIANA_IMAGE_REL_BASED_DIR64                 10


static
int DianaPeFile_ProcessRelocationEntry(DianaReadWriteRandomStream * pOutStream,
                                       OPERAND_SIZE moduleAddress,
                                       DI_UINT32 virtualAddress,
                                       DI_INT32 subEntriesCount,
                                       DI_UINT16 * pSubEntry,
                                       OPERAND_SIZE diff)
{
    OPERAND_SIZE targetAddress = 0;
    int i = 0;
    DI_UINT16 tempVal16 = 0;
    DI_UINT32 tempVal32 = 0;
    DI_UINT64 tempVal64 = 0;
    OPERAND_SIZE readBytes = 0;

    for(i = 0; i < subEntriesCount; ++i, ++pSubEntry)
    {
       DI_UINT16 offset = *pSubEntry & (DI_UINT16)0xfff;
       DI_UINT32 rawTarget = virtualAddress + offset;

       if (!rawTarget)
       {
           continue;
       }

       // calculate the target inside our mapped image
       targetAddress = moduleAddress;
       DI_CHECK(Diana_SafeAdd(&targetAddress, rawTarget));


       // done it
       switch ((*pSubEntry) >> 12) 
       {
            case DIANA_IMAGE_REL_BASED_HIGH:
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream, 
                                                       targetAddress,
                                                       &tempVal16,
                                                       sizeof(tempVal16),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal16), DI_ERROR);
                tempVal32 = tempVal16;
                tempVal32 <<= 16;
                tempVal32 += (DI_UINT32)diff;
                tempVal16 = (tempVal32 >> 16);
               
                DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                                       targetAddress,
                                                       &tempVal16,
                                                       sizeof(tempVal16),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal16), DI_ERROR);
                break;

            case DIANA_IMAGE_REL_BASED_HIGHLOW:
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream, 
                                                       targetAddress,
                                                       &tempVal32,
                                                       sizeof(tempVal32),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal32), DI_ERROR);
                tempVal32 += (DI_UINT32)diff;
               
                DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                                       targetAddress,
                                                       &tempVal32,
                                                       sizeof(tempVal32),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal32), DI_ERROR);
                break;

            case DIANA_IMAGE_REL_BASED_ABSOLUTE:
                break;

            case DIANA_IMAGE_REL_BASED_DIR64:
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream, 
                                                       targetAddress,
                                                       &tempVal64,
                                                       sizeof(tempVal64),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal64), DI_ERROR);
                tempVal64 +=diff;
               
                DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                                       targetAddress,
                                                       &tempVal64,
                                                       sizeof(tempVal64),
                                                       &readBytes,
                                                       0));
                DI_CHECK_CONDITION(readBytes == sizeof(tempVal64), DI_ERROR);

                break;
        }

    }
    return DI_SUCCESS;
}

static 
int DianaPeFile_FixRelocs(/* in */ Diana_PeFile * pPeFile,
                          /* in */ OPERAND_SIZE address,
                          /* inout */ DianaReadWriteRandomStream * pOutStream)
{
    int status = 0;
    void * pCapturedRelocs = 0;
    void * pCapturedRelocs_end = 0;
    OPERAND_SIZE oldBase = pPeFile->pImpl->imageBase;
    OPERAND_SIZE diff = address - oldBase;
    OPERAND_SIZE readBytes = 0;
    DIANA_IMAGE_DATA_DIRECTORY * pRelocationDirectory = &pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_BASERELOC];
    DIANA_IMAGE_BASE_RELOCATION * pRelocationEntry = 0;
    if (!pRelocationDirectory->Size || !pRelocationDirectory->VirtualAddress) 
    {
        // no relocations
        return DI_SUCCESS;
    }

    if (!diff)
    {
        return DI_SUCCESS;
    }
    pCapturedRelocs = DIANA_MALLOC(pRelocationDirectory->Size);
    if (!pCapturedRelocs)
    {
        return DI_OUT_OF_MEMORY;
    }
    DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream, 
                                           address + pRelocationDirectory->VirtualAddress,
                                           pCapturedRelocs,
                                           pRelocationDirectory->Size,
                                           &readBytes,
                                           0));
    DI_CHECK_CONDITION_GOTO(readBytes == pRelocationDirectory->Size, DI_ERROR);

    pCapturedRelocs_end = (char*)pCapturedRelocs + pRelocationDirectory->Size;

    pRelocationEntry = (DIANA_IMAGE_BASE_RELOCATION * )pCapturedRelocs;
    while ((char*)pRelocationEntry < (char*)pCapturedRelocs_end - sizeof(DIANA_IMAGE_BASE_RELOCATION))
    {
        DI_UINT16 * pFirstSubEntry =  (DI_UINT16*)((char*)pRelocationEntry + sizeof(DIANA_IMAGE_BASE_RELOCATION));
        int iSubEntriesCount  = (pRelocationEntry->SizeOfBlock - sizeof(DIANA_IMAGE_BASE_RELOCATION))/sizeof(DI_UINT16);
        
        if ((DI_UINT32)((char*)pCapturedRelocs_end - (char*)pRelocationEntry) < pRelocationEntry->SizeOfBlock)
        {
            status = DI_ERROR;
            goto cleanup;
        }
        DI_CHECK_GOTO(DianaPeFile_ProcessRelocationEntry(pOutStream,
                                                        address,
                                                        pRelocationEntry->VirtualAddress,
                                                        iSubEntriesCount,
                                                        pFirstSubEntry,
                                                        diff));
        pRelocationEntry = (DIANA_IMAGE_BASE_RELOCATION * )(pFirstSubEntry + iSubEntriesCount);
    }
cleanup:
    if (pCapturedRelocs)
    {
        DIANA_FREE(pCapturedRelocs);
    }
    return status;
}

int DianaPeFile_Map(/* in */ Diana_PeFile * pPeFile,
                    /* in */ DianaMovableReadStream * pStream,
                    /* in */ OPERAND_SIZE address,
                    /* inout */ DianaReadWriteRandomStream * pOutStream,
                    /* in */ void * pPage,
                    /* in */ int pageSize)
{
    int i = 0;
    OPERAND_SIZE lastSectionOffset = 0;
    OPERAND_SIZE sizeOfHeaders = pPeFile->pImpl->sizeOfHeaders;
    // map header
    if ((pPeFile->flags & DIANA_PE_FILE_FLAGS_FILE_MODE) == 0)
    {
        return DI_INVALID_INPUT;
    }
    if (sizeOfHeaders == 0 || sizeOfHeaders > pPeFile->pImpl->sizeOfModule)
    {
        return DI_ERROR;
    }
    if (pageSize < sizeof(OPERAND_SIZE))
    {
        return DI_ERROR;
    }
    DI_CHECK(pStream->pMoveTo(pStream, 
                              0));
    DI_CHECK(DianaStreams_CopyData(&pStream->parent,
                                   pOutStream,
                                   address,
                                   sizeOfHeaders,
                                   pPage,
                                   pageSize));

    // map sections
    for(i = 0; i < pPeFile->pImpl->capturedSectionCount; ++i)
    {
        OPERAND_SIZE sectionVA = 0, sectionVAEnd = 0;
        DIANA_IMAGE_SECTION_HEADER * pSectionHeader = pPeFile->pImpl->pCapturedSections + i;

        DI_UINT32 virtualSize = pSectionHeader->Misc.VirtualSize;
        DI_UINT32 sizeOfRawData = pSectionHeader->SizeOfRawData;

        if (sizeOfRawData > pPeFile->pImpl->sizeOfModule ||
            pSectionHeader->PointerToRawData > pPeFile->pImpl->sizeOfModule)
        {
            return DI_ERROR;
        }
        if (virtualSize == 0) 
        {
            virtualSize = sizeOfRawData;
        }
        if (pSectionHeader->PointerToRawData == 0) 
        {
            sizeOfRawData = 0;
        } 
        else if (sizeOfRawData > virtualSize) 
        {
            sizeOfRawData = virtualSize;
        }

        sectionVA = pSectionHeader->VirtualAddress;
        DI_CHECK(Diana_SafeAdd(&sectionVA, address));

        sectionVAEnd = sectionVA;
        DI_CHECK(Diana_SafeAdd(&sectionVAEnd, sizeOfRawData));

        if (sizeOfRawData != 0) 
        {
            DI_CHECK(pStream->pMoveTo(pStream, 
                                      pSectionHeader->PointerToRawData));

            DI_CHECK(DianaStreams_CopyData(&pStream->parent,
                                           pOutStream,
                                           sectionVA,
                                           sizeOfRawData,
                                           pPage,
                                           pageSize));
            
            lastSectionOffset = pSectionHeader->PointerToRawData + sizeOfRawData;
        }

        if (sizeOfRawData < virtualSize) 
        {
            // zero the tail
            DI_CHECK(DianaStreams_MemsetData(pOutStream,
                                             sectionVAEnd,
                                             virtualSize - sizeOfRawData,
                                             0,
                                             pPage,
                                             pageSize));

        }
    }

    DI_CHECK(DianaPeFile_FixRelocs(pPeFile,
                                   address,
                                   pOutStream));


    {
        // patch imagebase like loader does
        DI_UINT32 optHeaderOffset = (DI_UINT32)pPeFile->pImpl->dosHeader.e_lfanew + sizeof(DIANA_IMAGE_NT_HEADERS);
        DI_UINT32 imageBaseOffset = 0;
        switch(pPeFile->pImpl->dianaMode)
        {
        case DIANA_MODE32:
            imageBaseOffset = optHeaderOffset + DIANA_FIELD_OFFSET(DIANA_IMAGE_OPTIONAL_HEADER32, ImageBase);
            break;
        case DIANA_MODE64:
            imageBaseOffset = optHeaderOffset + DIANA_FIELD_OFFSET(DIANA_IMAGE_OPTIONAL_HEADER64, ImageBase);
            break;
        default:
            return DI_ERROR;
        }

        {
            OPERAND_SIZE absoluteImageBaseOffset = address + imageBaseOffset;
            OPERAND_SIZE writeBytes = 0;
            DI_CHECK(pOutStream->pRandomWrite(pOutStream, 
                                            absoluteImageBaseOffset,
                                            &address,
                                            pPeFile->pImpl->dianaMode,
                                            &writeBytes,
                                            0));
            DI_CHECK_CONDITION(writeBytes == pPeFile->pImpl->dianaMode, DI_ERROR);
        }

    }
    return DI_SUCCESS;
}

DIANA_IMAGE_SECTION_HEADER * DianaPeFile_FindSection(Diana_PeFile * pPeFile,
                                                     const char * pSectionName,
                                                     int nameSize)
{
    int i;
    for(i = 0; i < pPeFile->pImpl->capturedSectionCount; ++i)
    {
        DIANA_IMAGE_SECTION_HEADER * pSectionHeader = &pPeFile->pImpl->pCapturedSections[i];
        
        if (strncmp((char*)pSectionHeader->Name, pSectionName, nameSize) == 0)
        {
            return pSectionHeader;
        }
    }
    return 0;
}

// link imports
void DianaPeFile_LinkImports_Observer_Init(DianaPeFile_LinkImports_Observer * pObserver,
                                           DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_fnc queryFunctionByOrdinal,
                                           DianaPeFile_LinkImports_Observer_QueryFunctionByName_fnc queryFunctionByName)
{
    pObserver->queryFunctionByOrdinal = queryFunctionByOrdinal;
    pObserver->queryFunctionByName = queryFunctionByName;
}

#define DIANA_IMAGE_ORDINAL_FLAG64 0x8000000000000000ULL
#define DIANA_IMAGE_ORDINAL_FLAG32 0x80000000UL
#define DIANA_IMAGE_ORDINAL64(Ordinal) (Ordinal & 0xffff)
#define DIANA_IMAGE_ORDINAL32(Ordinal) (Ordinal & 0xffff)
#define DIANA_IMAGE_SNAP_BY_ORDINAL64(Ordinal) ((Ordinal & DIANA_IMAGE_ORDINAL_FLAG64) != 0)
#define DIANA_IMAGE_SNAP_BY_ORDINAL32(Ordinal) ((Ordinal & DIANA_IMAGE_ORDINAL_FLAG32) != 0)

int DianaPeFile_LinkDll32(OPERAND_SIZE address,
                          DianaReadWriteRandomStream * pOutStream,
                        const char * pDllName,
                        OPERAND_SIZE firstThunkOffset, 
                        void * pPage,
                        int pageSize,
                        void * pImageImportBuffer,
                        int imageImportBufferSize,
                        DianaPeFile_LinkImports_Observer * pObserver)
{
    PDIANA_IMAGE_THUNK_DATA32 p = 0;
    char * p_end = 0;
    OPERAND_SIZE readBytes = 0;
    OPERAND_SIZE currentThunkOffset = firstThunkOffset;
    int thunksCountInPage = pageSize/sizeof(DIANA_IMAGE_THUNK_DATA32);
    int usedPageSize = thunksCountInPage*sizeof(DIANA_IMAGE_THUNK_DATA32);
    if (thunksCountInPage == 0)
    {
        return DI_ERROR;
    }

    for(;;)
    {
        DI_CHECK(pOutStream->parent.pRandomRead(pOutStream,
                                                currentThunkOffset,
                                                pPage,
                                                usedPageSize,
                                                &readBytes,
                                                0));

        if (readBytes < sizeof(DIANA_IMAGE_THUNK_DATA32))
        {
            return DI_ERROR;
        }
        p_end = (char*)p + readBytes;
        for(p = (PDIANA_IMAGE_THUNK_DATA32)pPage;
            ((char*)p_end - (char*)p) > sizeof(DIANA_IMAGE_THUNK_DATA32);
            ++p,currentThunkOffset+=sizeof(DIANA_IMAGE_THUNK_DATA32))
        {
            OPERAND_SIZE function = 0;
            if (!p->u1.AddressOfData)
            {
                return DI_SUCCESS;
            }
            if (DIANA_IMAGE_SNAP_BY_ORDINAL32(p->u1.Ordinal))
            {
                DI_CHECK(pObserver->queryFunctionByOrdinal(pObserver,
                                                           pDllName,
                                                           DIANA_IMAGE_ORDINAL32(p->u1.Ordinal),
                                                           &function));
            }
            else
            {
                DIANA_IMAGE_IMPORT_BY_NAME * pImportByName = 0;
                OPERAND_SIZE functionNameAddress = address;
               
                DI_CHECK(Diana_SafeAdd(&functionNameAddress, p->u1.AddressOfData));
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream,
                                                        functionNameAddress,
                                                        pImageImportBuffer,
                                                        imageImportBufferSize,
                                                        &readBytes,
                                                        0));
                
                if (readBytes < sizeof(DIANA_IMAGE_IMPORT_BY_NAME))
                {
                    return DI_ERROR;
                }
                ((char*)pImageImportBuffer)[imageImportBufferSize-1] = 0;
                pImportByName = (DIANA_IMAGE_IMPORT_BY_NAME*)pImageImportBuffer;
                
                DI_CHECK(pObserver->queryFunctionByName(pObserver,
                                                        pDllName,
                                                        pImportByName->Name,
                                                        pImportByName->Hint,
                                                        &function));
            }
            {
                DI_UINT32 functionPtr = (DI_UINT32)function;
                DI_CHECK(pOutStream->pRandomWrite(pOutStream,
                                                currentThunkOffset,
                                                &functionPtr,
                                                sizeof(functionPtr),
                                                &readBytes,
                                                0));
            }

        }
        return DI_ERROR;
    }
}

int DianaPeFile_LinkDll64(OPERAND_SIZE address,
                          DianaReadWriteRandomStream * pOutStream,
                        const char * pDllName,
                        OPERAND_SIZE firstThunkOffset, 
                        void * pPage,
                        int pageSize,
                        void * pImageImportBuffer,
                        int imageImportBufferSize,
                        DianaPeFile_LinkImports_Observer * pObserver)
{
    PDIANA_IMAGE_THUNK_DATA64 p = 0;
    char * p_end = 0;
    OPERAND_SIZE readBytes = 0;
    OPERAND_SIZE currentThunkOffset = firstThunkOffset;
    int thunksCountInPage = pageSize/sizeof(DIANA_IMAGE_THUNK_DATA64);
    int usedPageSize = thunksCountInPage*sizeof(DIANA_IMAGE_THUNK_DATA64);
    if (thunksCountInPage == 0)
    {
        return DI_ERROR;
    }

    for(;;)
    {
        DI_CHECK(pOutStream->parent.pRandomRead(pOutStream,
                                                currentThunkOffset,
                                                pPage,
                                                usedPageSize,
                                                &readBytes,
                                                0));

        if (readBytes < sizeof(DIANA_IMAGE_THUNK_DATA64))
        {
            return DI_ERROR;
        }
        p_end = (char*)p + readBytes;
        for(p = (PDIANA_IMAGE_THUNK_DATA64)pPage;
            ((char*)p_end - (char*)p) > sizeof(DIANA_IMAGE_THUNK_DATA64);
            ++p,currentThunkOffset+=sizeof(DIANA_IMAGE_THUNK_DATA64))
        {
            OPERAND_SIZE function = 0;
            if (!p->u1.AddressOfData)
            {
                return DI_SUCCESS;
            }
            if (DIANA_IMAGE_SNAP_BY_ORDINAL64(p->u1.Ordinal))
            {
                DI_CHECK(pObserver->queryFunctionByOrdinal(pObserver,
                                                           pDllName,
                                                           DIANA_IMAGE_ORDINAL64(p->u1.Ordinal),
                                                           &function));
            }
            else
            {
                DIANA_IMAGE_IMPORT_BY_NAME * pImportByName = 0;
                OPERAND_SIZE functionNameAddress = address;
               
                DI_CHECK(Diana_SafeAdd(&functionNameAddress, p->u1.AddressOfData));
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream,
                                                        functionNameAddress,
                                                        pImageImportBuffer,
                                                        imageImportBufferSize,
                                                        &readBytes,
                                                        0));
                
                if (readBytes < sizeof(DIANA_IMAGE_IMPORT_BY_NAME))
                {
                    return DI_ERROR;
                }
                ((char*)pImageImportBuffer)[imageImportBufferSize-1] = 0;
                pImportByName = (DIANA_IMAGE_IMPORT_BY_NAME*)pImageImportBuffer;
                
                DI_CHECK(pObserver->queryFunctionByName(pObserver,
                                                        pDllName,
                                                        pImportByName->Name,
                                                        pImportByName->Hint,
                                                        &function));
            }
            {
                DI_CHECK(pOutStream->pRandomWrite(pOutStream,
                                                currentThunkOffset,
                                                &function,
                                                sizeof(function),
                                                &readBytes,
                                                0));
            }

        }
        return DI_ERROR;
    }
}
int DianaPeFile_LinkImports(/* in */ Diana_PeFile * pPeFile,
                            /* in */ OPERAND_SIZE address,
                            /* inout */ DianaReadWriteRandomStream * pOutStream,
                            /* in */ void * pPage,
                            /* in */ int pageSize,
                            /* in */ DianaPeFile_LinkImports_Observer * pObserver
                            )
{
#define MAX_DLL_NAME    512
#define MAX_IMAGE_IMPORT_BUFFER_SIZE   (MAX_DLL_NAME + sizeof(DIANA_IMAGE_IMPORT_BY_NAME))

    char * pDllNameBuffer = 0;
    char * pImageImportBuffer = 0;
    DIANA_IMAGE_DATA_DIRECTORY * pImportDirectory = &pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_IMPORT];
    int status = 0;
    void * pCapturedImports = 0;
    void * pCapturedImports_end = 0;
    long long debugCounter = 0;
    OPERAND_SIZE readBytes = 0;
    PDIANA_IMAGE_IMPORT_DESCRIPTOR pImportDescriptor = 0;
    if (!pImportDirectory->VirtualAddress || !pImportDirectory->Size)
    {
        return DI_SUCCESS;
    }

    pCapturedImports = DIANA_MALLOC(pImportDirectory->Size);
    if (!pCapturedImports)
    {
        return DI_OUT_OF_MEMORY;
    }
    pDllNameBuffer = DIANA_MALLOC(MAX_DLL_NAME);
    if (!pDllNameBuffer)
    {
        status = DI_OUT_OF_MEMORY;
        goto cleanup;
    }
    pImageImportBuffer = DIANA_MALLOC(MAX_IMAGE_IMPORT_BUFFER_SIZE);
    if (!pImageImportBuffer)
    {
        status = DI_OUT_OF_MEMORY;
        goto cleanup;
    }

    DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream, 
                                           address + pImportDirectory->VirtualAddress,
                                           pCapturedImports,
                                           pImportDirectory->Size,
                                           &readBytes,
                                           0));
    DI_CHECK_CONDITION_GOTO(readBytes == pImportDirectory->Size, DI_ERROR);

    pCapturedImports_end = (char*)pCapturedImports + pImportDirectory->Size - sizeof(DIANA_IMAGE_IMPORT_DESCRIPTOR) + 1;

    pImportDescriptor = (DIANA_IMAGE_IMPORT_DESCRIPTOR *)pCapturedImports;
    for(;(char*)pImportDescriptor < (char*)pCapturedImports_end; ++pImportDescriptor, ++debugCounter)
    {
        OPERAND_SIZE dllNameOffset = address;
        OPERAND_SIZE firstThunkOffset = address;
        DI_CHECK_GOTO(Diana_SafeAdd(&dllNameOffset, pImportDescriptor->Name));
        DI_CHECK_GOTO(Diana_SafeAdd(&firstThunkOffset, pImportDescriptor->FirstThunk));

        if (!pImportDescriptor->FirstThunk || !pImportDescriptor->Name)
        {
            break;
        }
        DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream,
                                                     dllNameOffset,
                                                     pDllNameBuffer,
                                                     MAX_DLL_NAME,
                                                     &readBytes,
                                                     0));
        DI_CHECK_CONDITION_GOTO(readBytes, DI_ERROR);
        pDllNameBuffer[MAX_DLL_NAME-1] = 0;



        switch(pPeFile->pImpl->dianaMode)
        {
        case DIANA_MODE32:
             DI_CHECK_GOTO(DianaPeFile_LinkDll32(address,
                                          pOutStream,
                                          pDllNameBuffer, 
                                          firstThunkOffset,
                                          pPage,
                                          pageSize,
                                          pImageImportBuffer,
                                          MAX_IMAGE_IMPORT_BUFFER_SIZE,
                                          pObserver));
             break;
        case DIANA_MODE64:
             DI_CHECK_GOTO(DianaPeFile_LinkDll64(address,
                                          pOutStream,
                                          pDllNameBuffer, 
                                          firstThunkOffset,
                                          pPage,
                                          pageSize,
                                          pImageImportBuffer,
                                          MAX_IMAGE_IMPORT_BUFFER_SIZE,
                                          pObserver));
             break;
        default:
            DI_CHECK_GOTO(DI_ERROR);
        }
    }
cleanup:
    if (pImageImportBuffer)
    {
        DIANA_FREE(pImageImportBuffer);
    }
    if (pDllNameBuffer)
    {
        DIANA_FREE(pDllNameBuffer);
    }
    if (pCapturedImports)
    {
        DIANA_FREE(pCapturedImports);
    }
    return status;

}

int DianaPeFile_ReadAllVirtual(/* in */ OPERAND_SIZE peStartAddress,
                                /* inout */ DianaReadWriteRandomStream * pOutStream,
                                /* in */ OPERAND_SIZE virtualAddress,
                                /* in */ OPERAND_SIZE sizeToRead,
                                /* out */ void ** ppSection
                               )
{
    int status = 0;
    void * pResult = 0;
    OPERAND_SIZE startOfRegion = 0;
    OPERAND_SIZE readBytes = 0;
    if (virtualAddress < peStartAddress)
        return DI_ERROR;
    
    *ppSection = 0;
    startOfRegion = peStartAddress;
    DI_CHECK(Diana_SafeAdd(&startOfRegion, virtualAddress));

    // allocate
    pResult = DIANA_MALLOC(sizeToRead);
    if (!pResult)
    {
        status = DI_OUT_OF_MEMORY;
        goto cleanup;
    }
    DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream, 
                                                 startOfRegion,
                                                 pResult,
                                                 (int)sizeToRead,
                                                 &readBytes,
                                                 0));
    if (readBytes != sizeToRead)
    {
        status = DI_ERROR;
        goto cleanup;
    }
    *ppSection = pResult;
    return DI_SUCCESS;
cleanup:
    if (pResult)
    {
        DIANA_FREE(pResult);
    }
    return status;
}


#define MAX_CALLBACKS_COUNT_SUPPORTED   64
static 
int DianaPeFile_ReadTLSCallbacks32(OPERAND_SIZE addressOfCallbacks,
                                   DianaReadWriteRandomStream * pOutStream,
                                   void ** ppTlsCallbacks,
                                   int * pCallbacksCount)
{
    int status = 0, i = 0;
    OPERAND_SIZE readBytes = 0;
    DI_UINT32 * pCallbacksArray = 0;

    *pCallbacksCount = 0;
    *ppTlsCallbacks = DIANA_MALLOC(MAX_CALLBACKS_COUNT_SUPPORTED*sizeof(DI_UINT32));
    if (!*ppTlsCallbacks)
    {
        return DI_OUT_OF_MEMORY;
    }
    DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream,
                                                 addressOfCallbacks,
                                                 *ppTlsCallbacks,
                                                 MAX_CALLBACKS_COUNT_SUPPORTED*sizeof(DI_UINT32),
                                                 &readBytes,
                                                 0));

    pCallbacksArray = *ppTlsCallbacks;
    for(i = 0; i < MAX_CALLBACKS_COUNT_SUPPORTED; ++i, ++*pCallbacksCount)
    {
        if (!pCallbacksArray[i])
        {
            break;
        }
    }
 cleanup:
    if (status && *ppTlsCallbacks)
    {
        DIANA_FREE(*ppTlsCallbacks);
        *ppTlsCallbacks = 0;
    }
    return status;

}
static 
int DianaPeFile_ReadTLSCallbacks64(OPERAND_SIZE addressOfCallbacks,
                                   DianaReadWriteRandomStream * pOutStream,
                                   void ** ppTlsCallbacks,
                                   int * pCallbacksCount)
{
    int status = 0, i = 0;
    OPERAND_SIZE readBytes = 0;
    DI_UINT64 * pCallbacksArray = 0;

    *pCallbacksCount = 0;
    *ppTlsCallbacks = DIANA_MALLOC(MAX_CALLBACKS_COUNT_SUPPORTED*sizeof(DI_UINT64));
    if (!*ppTlsCallbacks)
    {
        return DI_OUT_OF_MEMORY;
    }
    DI_CHECK_GOTO(pOutStream->parent.pRandomRead(pOutStream,
                                                 addressOfCallbacks,
                                                 *ppTlsCallbacks,
                                                 MAX_CALLBACKS_COUNT_SUPPORTED*sizeof(DI_UINT64),
                                                 &readBytes,
                                                 0));

    pCallbacksArray = *ppTlsCallbacks;
    for(i = 0; i < MAX_CALLBACKS_COUNT_SUPPORTED; ++i, ++*pCallbacksCount)
    {
        if (!pCallbacksArray[i])
        {
            break;
        }
    }
 cleanup:
    if (status && *ppTlsCallbacks)
    {
        DIANA_FREE(*ppTlsCallbacks);
        *ppTlsCallbacks = 0;
    }
    return status;

}

int DianaPeFile_QueryTLSCallbacks(/* in */ Diana_PeFile * pPeFile,
                                  /* in */ OPERAND_SIZE address,
                                  /* inout */ DianaReadWriteRandomStream * pOutStream,
                                  /* out */ void ** ppTlsCallbacks,
                                  /* out */ int * pCallbacksCount,
                                  /* out */ OPERAND_SIZE * pAddressOfTLSIndex)
{
    OPERAND_SIZE callbacksAddress = 0;
    int status = 0;
    void * pSection = 0;
    DIANA_IMAGE_DATA_DIRECTORY * pTLSDirectory = &pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_TLS];

    *pAddressOfTLSIndex = 0;
    *ppTlsCallbacks = 0;
    *pCallbacksCount = 0;
    if (!pTLSDirectory->VirtualAddress || !pTLSDirectory->Size)
    {
        return DI_SUCCESS;
    }

    DI_CHECK(DianaPeFile_ReadAllVirtual(address, 
                                        pOutStream, 
                                        pTLSDirectory->VirtualAddress, 
                                        pTLSDirectory->Size,
                                        &pSection));


    switch(pPeFile->pImpl->dianaMode)
    {
        case DIANA_MODE32:
            if (pTLSDirectory->Size < sizeof(DIANA_IMAGE_TLS_DIRECTORY32))
            {
                DI_CHECK_GOTO(DI_ERROR);
            }
            callbacksAddress = ((DIANA_IMAGE_TLS_DIRECTORY32*)pTLSDirectory)->AddressOfCallBacks;
            *pAddressOfTLSIndex = ((DIANA_IMAGE_TLS_DIRECTORY32*)pTLSDirectory)->AddressOfIndex;

            DI_CHECK_GOTO(DianaPeFile_ReadTLSCallbacks32(callbacksAddress,
                                                         pOutStream,
                                                         ppTlsCallbacks,
                                                         pCallbacksCount));
            break;
        case DIANA_MODE64:
            if (pTLSDirectory->Size < sizeof(DIANA_IMAGE_TLS_DIRECTORY64))
            {
                DI_CHECK_GOTO(DI_ERROR);
            }
            callbacksAddress = ((DIANA_IMAGE_TLS_DIRECTORY64*)pTLSDirectory)->AddressOfCallBacks;
            *pAddressOfTLSIndex = ((DIANA_IMAGE_TLS_DIRECTORY64*)pTLSDirectory)->AddressOfIndex;
            DI_CHECK_GOTO(DianaPeFile_ReadTLSCallbacks64(callbacksAddress,
                                                         pOutStream,
                                                         ppTlsCallbacks,
                                                         pCallbacksCount));
            break;
        default:
            DI_CHECK_GOTO(DI_ERROR);
    }

cleanup:
    if (pSection)
    {
        DIANA_FREE(pSection);
    }
    return status;
}


static 
int ValidateVirtualAddress(DI_UINT32 virtualAddress,
                           const char * pCapturedDataStart,
                           const char * pCapturedDataEnd,
                           const char ** ppData)
{
    OPERAND_SIZE moduleSize = pCapturedDataEnd - pCapturedDataStart;
    *ppData = 0;
    if (virtualAddress == 0)
    {
        return DI_ERROR;
    }
    if (virtualAddress >= moduleSize)
    {
        return DI_ERROR;
    }
    *ppData = pCapturedDataStart + virtualAddress;
    return DI_SUCCESS;
}
static 
int CaptureValue32(const char * pTable,
                   const char * pTableEnd,
                   DI_UINT32 offset,
                   DI_UINT32 * pResult)
{
    OPERAND_SIZE tableSize = pTableEnd - pTable;
    if (tableSize < 4)
    {
        return DI_ERROR;
    }
    tableSize -= 4;
    if ((OPERAND_SIZE)offset >= tableSize)
    {
        return DI_ERROR;
    }
    if ((OPERAND_SIZE)offset*4 >= tableSize)
    {
        return DI_ERROR;
    }
    *pResult = *(DI_UINT32*)(pTable + offset*4);
    return DI_SUCCESS;
}

static 
int CaptureValue16(const char * pTable,
                   const char * pTableEnd,
                   DI_UINT32 offset,
                   DI_UINT32 * pResult)
{
    OPERAND_SIZE tableSize = pTableEnd - pTable;
    if (tableSize < 2)
    {
        return DI_ERROR;
    }
    tableSize -= 2;
    if ((OPERAND_SIZE)offset >= tableSize)
    {
        return DI_ERROR;
    }
    if ((OPERAND_SIZE)offset*2 >= tableSize)
    {
        return DI_ERROR;
    }
    *pResult = *(DI_UINT16*)(pTable + offset*2);
    return DI_SUCCESS;
}

int DianaPeFile_GetProcAddress(Diana_PeFile * pPeFile,
                                const char * pCapturedDataStart,
                                const char * pCapturedDataEnd,
                                const char * pFunctionName,
                                OPERAND_SIZE * pFunctionOffset,
                                OPERAND_SIZE * pForwardInformationOffset)
{
    OPERAND_SIZE moduleSize = pCapturedDataEnd - pCapturedDataStart;
    const DIANA_IMAGE_DATA_DIRECTORY * pExportDirectory = &pPeFile->pImpl->pImageDataDirectoryArray[DIANA_IMAGE_DIRECTORY_ENTRY_EXPORT];
    const DIANA_IMAGE_EXPORT_DIRECTORY * pCapturedExportDirectory = 0;

    *pFunctionOffset = 0;
    *pForwardInformationOffset = 0;
    if (pPeFile->flags & DIANA_PE_FILE_FLAGS_FILE_MODE)
    {
        // map it somewhere first
        return DI_INVALID_INPUT;
    }
    if (!pExportDirectory->VirtualAddress || !pExportDirectory->Size)
    {
        return DI_NOT_FOUND;
    }

    if (pExportDirectory->VirtualAddress > moduleSize )
    {
        return DI_ERROR;
    }
    if (pExportDirectory->Size > moduleSize )
    {
        return DI_ERROR;
    }
    if (pExportDirectory->VirtualAddress > (moduleSize  - pExportDirectory->Size))
    {
        return DI_ERROR;
    }
    
    pCapturedExportDirectory = (const DIANA_IMAGE_EXPORT_DIRECTORY * )(pCapturedDataStart + pExportDirectory->VirtualAddress);
    if (!pCapturedExportDirectory->NumberOfNames)
    {
        return DI_ERROR;
    }

    {
    const char * pNameTable = 0;
    const char * pNameOrdinalTable = 0;
    const char * pFunctionsTable = 0;

    DI_UINT32 low = 0;
    DI_UINT32 middle = 0;
    DI_UINT32 high = pCapturedExportDirectory->NumberOfNames - 1;

    // get the tables
    DI_CHECK(ValidateVirtualAddress(pCapturedExportDirectory->AddressOfNames,
                                    pCapturedDataStart,
                                    pCapturedDataEnd,
                                    &pNameTable));
    DI_CHECK(ValidateVirtualAddress(pCapturedExportDirectory->AddressOfNameOrdinals,
                                    pCapturedDataStart,
                                    pCapturedDataEnd,
                                    &pNameOrdinalTable));
    DI_CHECK(ValidateVirtualAddress(pCapturedExportDirectory->AddressOfFunctions,
                                    pCapturedDataStart,
                                    pCapturedDataEnd,
                                    &pFunctionsTable));


    while (high >= low && (DI_INT32)high >= 0) 
    {
        int compareResult = 0;
        DI_UINT32 functionNameOffset = 0;
        const char * pFunctionPointer = 0;
        middle = (low + high) >> 1;

        DI_CHECK(CaptureValue32(pNameTable, pCapturedDataEnd, middle, &functionNameOffset));

        DI_CHECK(ValidateVirtualAddress(functionNameOffset,
                                        pCapturedDataStart,
                                        pCapturedDataEnd,
                                        &pFunctionPointer));

        compareResult = DIANA_STRNICMP(pFunctionName, pFunctionPointer, pCapturedDataEnd-pFunctionPointer);
        if (!compareResult)
        {
            break;
        }
        if (compareResult > 0)
        {
            low = middle + 1;
            continue;
        }
        high = middle - 1;
    }

    if ((DI_INT32)high < (DI_INT32)low)
    {
        return DI_NOT_FOUND;
    }


    {
        DI_UINT32 ordinal = 0;
        DI_UINT32 functionOffset = 0;
        DI_CHECK(CaptureValue16(pNameOrdinalTable, pCapturedDataEnd, middle, &ordinal));
        DI_CHECK(CaptureValue32(pFunctionsTable, pCapturedDataEnd, ordinal, &functionOffset));

        if ((functionOffset >= pExportDirectory->VirtualAddress) && (functionOffset < (pExportDirectory->VirtualAddress + pExportDirectory->Size)))
        {
            const char * p = pCapturedDataStart;
            for(; p < pCapturedDataEnd; ++p)
            {
                if (!*p)
                {
                    *pForwardInformationOffset = functionOffset;
                    return DI_SUCCESS;
                }
            }
            return DI_ERROR;
        }
        *pFunctionOffset = functionOffset;
        return DI_SUCCESS;
    }
    }
}
