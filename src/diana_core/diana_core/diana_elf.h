#ifndef DIANA_ELF_H
#define DIANA_ELF_H

#include "diana_elfs_defs.h"
#include "diana_streams.h"
#include "diana_analyze.h"
#include "diana_uids.h"
#include "diana_pe.h"

typedef struct _diana_ElfSectionWithInfo
{
    DIANA_ELF_SECTION_HEADER header;
    char sh_name_str[64];
} Diana_ElfSectionWithInfo;

#define DIANA_ELF_INTERNAL_FLAG_64BIT   1
#define DIANA_ELF_INTERNAL_FLAG_LE      2

typedef struct _diana_ElfFile_impl
{
    DIANA_ELF_HEADER elfHeader;
    DIANA_ELF_PROGRAM_HEADER* pCapturedSegments;
    int capturedSegmentCount;
    Diana_ElfSectionWithInfo* pCapturedSections;
    int capturedSectionCount;

    Diana_ElfSectionWithInfo* pSymbolTableSection;
    Diana_ElfSectionWithInfo* pDynamicSymbolTableSection;
    Diana_ElfSectionWithInfo* pStringTableSection;
    Diana_ElfSectionWithInfo* pDynamicStringTableSection;

    OPERAND_SIZE sizeOfModule;
    uint64_t dynamicAddress;
    uint64_t dynamicSize;
    int internalFlags;
}Diana_ElfFile_impl;
typedef struct _diana_ElfFile
{
    Diana_ElfFile_impl* pImpl;
    int flags;
    int dianaMode;
}
Diana_ElfFile;

// Flags:
#define DIANA_ELF_FILE_FLAGS_MODULE_MODE     0
#define DIANA_ELF_FILE_FLAGS_FILE_MODE       1

int DianaElfFile_Init(/* out */ Diana_ElfFile* pElfFile,
    /* in */ DianaMovableReadStream* pStream,
    /* in, optional */ OPERAND_SIZE sizeOfFile,
    /* in*/ int flags);

void DianaElfFile_Free(Diana_ElfFile* pElfFile);


int DianaElfFile_GetProcAddress(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    const char* symbolName,
    OPERAND_SIZE* pSymbolAddress);


int DianaElfFile_QueryImports(Diana_ElfFile* pElfFile,
    OPERAND_SIZE baseAddress,
    DianaReadWriteRandomStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags,
    int importFlags);


int DianaElfFile_QueryExports(Diana_ElfFile* pPeFile,
    DianaMovableReadStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags);


// Flag: skip relocation step after mapping
#define DIANA_ELF_MAP_DO_NOT_RELOCATE  1


int DianaElfFile_MapEx(/* in */  Diana_ElfFile* pElfFile,
    /* in */  DianaMovableReadStream* pStream,
    /* in */  OPERAND_SIZE                 address,
    /* inout*/DianaReadWriteRandomStream* pOutStream,
    /* in */  void* pPage,
    /* in */  int                          pageSize,
    /* in */  int                          flags);

#endif