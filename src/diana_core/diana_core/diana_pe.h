#ifndef DIANA_PE_H
#define DIANA_PE_H

#include "diana_pe_defs.h"
#include "diana_streams.h"
#include "diana_analyze.h"
#include "diana_uids.h"

typedef struct _diana_PeFile_impl
{
    // basic
    DIANA_IMAGE_DOS_HEADER dosHeader;
    DIANA_IMAGE_NT_HEADERS ntHeaders;
    DIANA_IMAGE_SECTION_HEADER * pCapturedSections;
    int capturedSectionCount;

    // advanced
    int dianaMode;
    OPERAND_SIZE sizeOfModule;
    OPERAND_SIZE sizeOfInitialFile;
    OPERAND_SIZE sizeOfImpl;
    int optionalHeaderSize;
    DI_UINT32  addressOfEntryPoint;
    DIANA_IMAGE_DATA_DIRECTORY * pImageDataDirectoryArray;
    DI_UINT32  sizeOfHeaders;
    OPERAND_SIZE   imageBase;
}Diana_PeFile_impl;

typedef struct _diana_PeFile32_impl
{
    Diana_PeFile_impl parent;
    DIANA_IMAGE_OPTIONAL_HEADER32 * pOptionalHeader;
}
Diana_PeFile32_impl;

typedef struct _diana_PeFile64_impl
{
    Diana_PeFile_impl parent;
    DIANA_IMAGE_OPTIONAL_HEADER64 * pOptionalHeader;
}
Diana_PeFile64_impl;


// Flags:
#define DIANA_PE_FILE_FLAGS_MODULE_MODE     0
#define DIANA_PE_FILE_FLAGS_FILE_MODE       1


typedef struct _diana_PeFile
{
    Diana_PeFile_impl  * pImpl;
    int flags;
}
Diana_PeFile;

int DianaPeFile_Init(/* out */ Diana_PeFile * pPeFile,
                      /* in */ DianaMovableReadStream * pStream,
                      /* in, optional */ OPERAND_SIZE sizeOfFile,
                      /* in*/ int flags);

void DianaPeFile_Free(Diana_PeFile * pPeFile);


int DianaPeFile_Map(/* in */ Diana_PeFile * pPeFile,
                    /* in */ DianaMovableReadStream * pStream,
                    /* in */ OPERAND_SIZE address,
                    /* inout */ DianaReadWriteRandomStream * pOutStream,
                    /* in */ void * pPage,
                    /* in */ int pageSize);
DIANA_IMAGE_SECTION_HEADER * DianaPeFile_FindSection(Diana_PeFile * pPeFile,
                                                     const char * pSectionName,
                                                     int nameSize);



typedef int (* DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_fnc)(void * pThis, 
                                                                            const char * pDllName,
                                                                            DI_UINT32 ordinal,
                                                                            OPERAND_SIZE * pAddress);

typedef int (* DianaPeFile_LinkImports_Observer_QueryFunctionByName_fnc)(void * pThis, 
                                                                         const char * pDllName,
                                                                         const char * pFunctionName,
                                                                         DI_UINT32 hint,
                                                                         OPERAND_SIZE * pAddress);

typedef struct _DianaPeFile_LinkImports_Observer
{
    DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_fnc queryFunctionByOrdinal;
    DianaPeFile_LinkImports_Observer_QueryFunctionByName_fnc queryFunctionByName;
}
DianaPeFile_LinkImports_Observer;

    
void DianaPeFile_LinkImports_Observer_Init(DianaPeFile_LinkImports_Observer * pObserver,
                                           DianaPeFile_LinkImports_Observer_QueryFunctionByOrdinal_fnc queryFunctionByOrdinal,
                                           DianaPeFile_LinkImports_Observer_QueryFunctionByName_fnc queryFunctionByName);

int DianaPeFile_LinkImports(/* in */ Diana_PeFile * pPeFile,
                            /* in */ OPERAND_SIZE address,
                            /* inout */ DianaReadWriteRandomStream * pOutStream,
                            /* in */ void * pPage,
                            /* in */ int pageSize,
                            /* in */ DianaPeFile_LinkImports_Observer * pObserver
                            );
int DianaPeFile_QueryGUID(/* in */ Diana_PeFile* pPeFile,
                          /* inout */ DianaMovableReadStream* pOutStream,
                          /* in */ OPERAND_SIZE address,
                          /* out */ DIANA_UUID* pPdbUID);

int DianaPeFile_ReadAllVirtual(/* in */ OPERAND_SIZE peStartAddress,
                                /* inout */ DianaReadWriteRandomStream * pOutStream,
                                /* in */ OPERAND_SIZE virtualAddress,
                                /* in */ OPERAND_SIZE sizeToRead,
                                /* out */ void ** ppSection
                               );

int DianaPeFile_QueryTLSCallbacks(/* in */ Diana_PeFile * pPeFile,
                            /* in */ OPERAND_SIZE address,
                            /* inout */ DianaReadWriteRandomStream * pOutStream,
                            /* out */ void ** ppTlsCallbacks,
                            /* out */ int * callbacksCount,
                            /* out */ OPERAND_SIZE * pAddressOfTLSIndex
                            );
int DianaPeFile_GetProcAddress(Diana_PeFile * pPeFile,
                                const char * pCapturedDataStart,
                                const char * pCapturedDataEnd,
                                const char * pFunctionName,
                                OPERAND_SIZE * pFunctionOffset,
                                OPERAND_SIZE * pForwardInformationOffset);


#define DIANA_IMAGE_DEBUG_TYPE_UNKNOWN      0
#define DIANA_IMAGE_DEBUG_TYPE_COFF         1
#define DIANA_IMAGE_DEBUG_TYPE_CODEVIEW     2
#define DIANA_IMAGE_DEBUG_TYPE_FPO          3
#define DIANA_IMAGE_DEBUG_TYPE_MISC         4
#define DIANA_IMAGE_DEBUG_TYPE_EXCEPTION    5
#define DIANA_IMAGE_DEBUG_TYPE_FIXUP        6
#define DIANA_IMAGE_DEBUG_TYPE_BORLAND      9


#endif