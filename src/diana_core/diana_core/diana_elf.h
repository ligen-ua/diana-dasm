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
    DI_UINT64 dynamicAddress;
    DI_UINT64 dynamicSize;
    int internalFlags;
}Diana_ElfFile_impl;
typedef struct _diana_ElfFile
{
    Diana_ElfFile_impl* pImpl;
    int flags;
    int dianaMode;  // the pointer size
    int arch;       // DIANA_ARCH_*, from e_machine
}
Diana_ElfFile;

// Flags:
#define DIANA_ELF_FILE_FLAGS_MODULE_MODE     DIANA_EXECUTABLE_FILE_FLAGS_MODULE_MODE
#define DIANA_ELF_FILE_FLAGS_FILE_MODE       DIANA_EXECUTABLE_FILE_FLAGS_FILE_MODE


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


typedef int (*DianaElfFile_NeededLibrary_Callback)(void* pContext, const char* libName);

int DianaElfFile_GetNeededLibraries(/* in */ Diana_ElfFile* pElfFile,
    /* in */ DianaMovableReadStream* pStream,
    /* in */ DianaElfFile_NeededLibrary_Callback callback,
    /* in */ void* pContext,
    /* in */ int streamFlags);

/* Startup routines: the code the dynamic linker runs before main, the ELF counterpart of PE TLS callbacks */
#define DIANA_ELF_STARTUP_IFUNC          1  /* an IFUNC resolver: a STT_GNU_IFUNC symbol or an R_*_IRELATIVE relocation */
#define DIANA_ELF_STARTUP_PREINIT_ARRAY  2  /* an entry of DT_PREINIT_ARRAY */
#define DIANA_ELF_STARTUP_INIT           3  /* DT_INIT */
#define DIANA_ELF_STARTUP_INIT_ARRAY     4  /* an entry of DT_INIT_ARRAY */

typedef struct _DianaElfStartupRoutine
{
    int kind;                   /* DIANA_ELF_STARTUP_* */
    int index;                  /* the array position; for IFUNCs without a name, their number */
    OPERAND_SIZE offset;        /* the routine as a module offset (a link-time address, like st_value) */
    OPERAND_SIZE slotOffset;    /* the GOT slot an IRELATIVE fills or the array entry; 0 otherwise */
    const char* pName;          /* the IFUNC symbol name or 0; valid during the callback only */
} DianaElfStartupRoutine;

typedef int (*DianaElfFile_StartupRoutine_Callback)(void* pContext, const DianaElfStartupRoutine* pRoutine);

/* Converts a pointer stored in the image to a module offset */
typedef int (*DianaElfFile_PointerToOffset_Callback)(void* pContext, OPERAND_SIZE pointer, OPERAND_SIZE* pOffset);

/* Reports IFUNC resolvers first (named ones, then the IRELATIVE-only ones), then the
   initializers in the order the loader calls them.
   pStream is a module-mode stream; reads use module offsets and streamFlags.
   moduleAddress: the loader relocates some pointers in the image (most dynamic tags, the
   init arrays) and leaves the others at their link-time addresses. A pointer within
   [moduleAddress, moduleAddress + sizeOfModule) is taken as relocated. pointerToOffset,
   when given, does this conversion instead. */
int DianaElfFile_QueryStartupRoutines(/* in */ Diana_ElfFile* pElfFile,
    /* in */ DianaMovableReadStream* pStream,
    /* in */ OPERAND_SIZE moduleAddress,
    /* in, optional */ DianaElfFile_PointerToOffset_Callback pointerToOffset,
    /* in, optional */ void* pPointerContext,
    /* in */ DianaElfFile_StartupRoutine_Callback callback,
    /* in */ void* pContext,
    /* in */ int streamFlags);

// Scans the PT_NOTE segments for an NT_GNU_BUILD_ID note (name "GNU") and
// returns its descriptor bytes (the raw build-id, typically 20 bytes for a
// SHA1-based id). Returns DI_NOT_FOUND if no such note is present.
// `address` is added to each segment's p_vaddr before reading, same
// convention as DianaElfFile_MapEx (0 when pOutStream is already rooted at
// the module's base, e.g. a module-mode stream over a live process image).
int DianaElfFile_QueryBuildId(/* in */ Diana_ElfFile* pElfFile,
    /* inout */ DianaMovableReadStream* pOutStream,
    /* in */ OPERAND_SIZE address,
    /* out */ DI_UINT8* pBuildIdBuffer,
    /* in */ DI_UINT32 bufferSize,
    /* out */ DI_UINT32* pBuildIdSize);

#endif