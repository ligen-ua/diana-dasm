#include "diana_elf.h"


static
int ReadSectionHeaders(Diana_ElfFile_impl* pImpl,
    DianaMovableReadStream* pStream,
    OPERAND_SIZE sizeOfFile)
{
    char* pStringTable = 0;
    Diana_ElfSectionWithInfo* pSectionHeader = 0;
    int i;
    OPERAND_SIZE shAddress = 0;
    int status = 0;

    if (pImpl->elfHeader.e_shnum == 0)
        return DI_SUCCESS;

    if (pImpl->elfHeader.e_shoff >= sizeOfFile)
        return DI_INVALID_INPUT;

    // Allocate space for section headers
    pSectionHeader = DIANA_MALLOC(pImpl->elfHeader.e_shnum * sizeof(Diana_ElfSectionWithInfo));
    DI_CHECK_ALLOC(pSectionHeader);

    DIANA_MEMSET(pSectionHeader, 0, pImpl->elfHeader.e_shnum * sizeof(Diana_ElfSectionWithInfo));
    pImpl->pCapturedSections = pSectionHeader;

    // Read all section headers at once
    shAddress = pImpl->elfHeader.e_shoff;
    for (i = 0; i < pImpl->elfHeader.e_shnum; ++i)
    {
        DIANA_ELF_SECTION_HEADER * pHeader = &pSectionHeader[i].header;
        DI_CHECK_GOTO(pStream->pMoveTo(pStream, shAddress));

        DI_CHECK_GOTO(DianaExactRead(&pStream->parent,
            pHeader,
            sizeof(DIANA_ELF_SECTION_HEADER)));

        DI_CHECK_GOTO(Diana_SafeAdd(&shAddress, pImpl->elfHeader.e_shentsize));
    }

    pImpl->capturedSectionCount = pImpl->elfHeader.e_shnum;

    // Now read the section name string table (.shstrtab)
    // It's at index e_shstrndx
    if (pImpl->elfHeader.e_shstrndx == ((DI_UINT16)0xffff))
    {
        pImpl->elfHeader.e_shstrndx = (DI_UINT16)pSectionHeader[0].header.sh_link;
    }

    if (pImpl->elfHeader.e_shstrndx > 0 && pImpl->elfHeader.e_shstrndx < pImpl->elfHeader.e_shnum)
    {
        DIANA_SIZE_T strTabAllocSize = 0;
        OPERAND_SIZE strTabOpSize = 0;
        DIANA_ELF_SECTION_HEADER* pStrTabSection =
            &pSectionHeader[pImpl->elfHeader.e_shstrndx].header;

        if (pStrTabSection->sh_size == 0 || pStrTabSection->sh_size > DIANA_MAX_SAFE_ALLOC_SIZE)
            return DI_INVALID_INPUT;

        strTabOpSize = pStrTabSection->sh_size;
        DI_CHECK_GOTO(Diana_ConvertOpSizeToSizeT(&strTabOpSize, &strTabAllocSize));
        
        pStringTable = DIANA_MALLOC(strTabAllocSize);
        DI_CHECK_ALLOC(pStringTable);

        // Read the string table
        DI_CHECK_GOTO(pStream->pMoveTo(pStream, pStrTabSection->sh_offset));
        DI_CHECK_GOTO(DianaExactReadSafe(&pStream->parent, pStringTable, pStrTabSection->sh_size));

        // Now fill in section names from the string table
        for (i = 0; i < pImpl->capturedSectionCount; ++i)
        {
            Diana_ElfSectionWithInfo* pSection = &pImpl->pCapturedSections[i];
            DI_UINT32 nameOffset = pSection->header.sh_name;

            if (nameOffset < pStrTabSection->sh_size)
            {
                // Copy section name (up to 63 chars to fit in our buffer)
                const char* pName = &pStringTable[nameOffset];
                int nameLen = 0;

                while (nameLen < 63 &&
                       (nameOffset + (DI_UINT32)nameLen) < pStrTabSection->sh_size &&
                       pName[nameLen] != '\0')
                    nameLen++;

                DIANA_MEMSET(pSection->sh_name_str, 0, sizeof(pSection->sh_name_str));
                DIANA_MEMCPY(pSection->sh_name_str, pName, nameLen);
            }
        }
    }

cleanup:
    if (pStringTable)
        DIANA_FREE(pStringTable);

    return status;
}

static
int InitializeSymbolTables(Diana_ElfFile_impl* pImpl)
{
    int i;

    for (i = 0; i < pImpl->capturedSectionCount; ++i)
    {
        Diana_ElfSectionWithInfo* pSection = &pImpl->pCapturedSections[i];

        if (pSection->header.sh_type == DIANA_SHT_SYMTAB)
            pImpl->pSymbolTableSection = pSection;
        else if (pSection->header.sh_type == DIANA_SHT_DYNSYM)
            pImpl->pDynamicSymbolTableSection = pSection;
        else if (pSection->header.sh_type == DIANA_SHT_STRTAB)
        {
            // Determine if this is the main string table or dynamic string table
            if (pSection- pImpl->pCapturedSections == pImpl->elfHeader.e_shstrndx)
                pImpl->pStringTableSection = pSection;
            else
                pImpl->pDynamicStringTableSection = pSection;
        }
    }

    return DI_SUCCESS;
}

static DI_UINT16 DianaElf_rd16(const unsigned char* p, int isLE)
{
    if (isLE) return (DI_UINT16)p[0] | ((DI_UINT16)p[1] << 8);
    return (DI_UINT16)p[1] | ((DI_UINT16)p[0] << 8);
}

static DI_UINT32 DianaElf_rd32(const unsigned char* p, int isLE)
{
    if (isLE)
        return (DI_UINT32)p[0] |
        ((DI_UINT32)p[1] << 8) |
        ((DI_UINT32)p[2] << 16) |
        ((DI_UINT32)p[3] << 24);
    return (DI_UINT32)p[3] |
        ((DI_UINT32)p[2] << 8) |
        ((DI_UINT32)p[1] << 16) |
        ((DI_UINT32)p[0] << 24);
}

static DI_UINT64 DianaElf_rd64(const unsigned char* p, int isLE)
{
    if (isLE)
        return (DI_UINT64)p[0] |
        ((DI_UINT64)p[1] << 8) |
        ((DI_UINT64)p[2] << 16) |
        ((DI_UINT64)p[3] << 24) |
        ((DI_UINT64)p[4] << 32) |
        ((DI_UINT64)p[5] << 40) |
        ((DI_UINT64)p[6] << 48) |
        ((DI_UINT64)p[7] << 56);
    return (DI_UINT64)p[7] |
        ((DI_UINT64)p[6] << 8) |
        ((DI_UINT64)p[5] << 16) |
        ((DI_UINT64)p[4] << 24) |
        ((DI_UINT64)p[3] << 32) |
        ((DI_UINT64)p[2] << 40) |
        ((DI_UINT64)p[1] << 48) |
        ((DI_UINT64)p[0] << 56);
}


static int DianaElf_DecodePhdrNormalized(DIANA_ELF_PROGRAM_HEADER* out,
    const unsigned char* raw,
    int rawSize,
    int is64bit,
    int isLE)
{
    // Minimum sizes per ABI:
    // Elf32_Phdr is 32 bytes, Elf64_Phdr is 56 bytes. [web:6][web:10]
    if (is64bit)
    {
        if (rawSize < 56) return DI_INVALID_INPUT;

        out->p_type = DianaElf_rd32(raw + 0x00, isLE);
        out->p_flags = DianaElf_rd32(raw + 0x04, isLE);
        out->p_offset = DianaElf_rd64(raw + 0x08, isLE);
        out->p_vaddr = DianaElf_rd64(raw + 0x10, isLE);
        out->p_paddr = DianaElf_rd64(raw + 0x18, isLE);
        out->p_filesz = DianaElf_rd64(raw + 0x20, isLE);
        out->p_memsz = DianaElf_rd64(raw + 0x28, isLE);
        out->p_align = DianaElf_rd64(raw + 0x30, isLE);
        return DI_SUCCESS;
    }
    else
    {
        if (rawSize < 32) return DI_INVALID_INPUT;

        out->p_type = DianaElf_rd32(raw + 0x00, isLE);
        out->p_offset = (DI_UINT64)DianaElf_rd32(raw + 0x04, isLE);
        out->p_vaddr = (DI_UINT64)DianaElf_rd32(raw + 0x08, isLE);
        out->p_paddr = (DI_UINT64)DianaElf_rd32(raw + 0x0C, isLE);
        out->p_filesz = (DI_UINT64)DianaElf_rd32(raw + 0x10, isLE);
        out->p_memsz = (DI_UINT64)DianaElf_rd32(raw + 0x14, isLE);
        out->p_flags = DianaElf_rd32(raw + 0x18, isLE);
        out->p_align = (DI_UINT64)DianaElf_rd32(raw + 0x1C, isLE);
        return DI_SUCCESS;
    }
}


static
int ReadProgramHeaders(Diana_ElfFile_impl* pImpl,
    DianaMovableReadStream* pStream,
    OPERAND_SIZE sizeOfFile)
{
    DIANA_ELF_PROGRAM_HEADER* pSegmentHeader = 0;
    int i;

    // You need these normalized header fields available.
    // If you still store "elfHeader.e_phnum/e_phoff/e_phentsize" directly, keep using that.
    const DI_UINT16 phnum = pImpl->elfHeader.e_phnum;
    const OPERAND_SIZE phoff = pImpl->elfHeader.e_phoff;
    const DI_UINT16 phentsize = pImpl->elfHeader.e_phentsize;
    int status = DI_SUCCESS;
    unsigned char* tmp = 0;

    if (phnum == 0)
        return DI_SUCCESS;

    // Bounds (avoid overflow: phoff + phnum*phentsize <= sizeOfFile)
    if (sizeOfFile && phoff >= sizeOfFile)
        return DI_INVALID_INPUT;

    {
        OPERAND_SIZE tableSize = (OPERAND_SIZE)phnum * (OPERAND_SIZE)phentsize;
        OPERAND_SIZE end = phoff;
        DI_CHECK(Diana_SafeAdd(&end, tableSize));
        if (sizeOfFile && end > sizeOfFile)
            return DI_INVALID_INPUT;
    }

    // Allocate normalized array
    pSegmentHeader = (DIANA_ELF_PROGRAM_HEADER*)DIANA_MALLOC((DIANA_SIZE_T)phnum * sizeof(DIANA_ELF_PROGRAM_HEADER));
    DI_CHECK_ALLOC(pSegmentHeader);

    DIANA_MEMSET(pSegmentHeader, 0, (DIANA_SIZE_T)phnum * sizeof(DIANA_ELF_PROGRAM_HEADER));
    pImpl->pCapturedSegments = pSegmentHeader;
    pImpl->capturedSegmentCount = 0;

    // Read entries
    DI_CHECK(pStream->pMoveTo(pStream, phoff));

    // temp buffer sized exactly as on disk
    tmp = (unsigned char*)DIANA_MALLOC(phentsize);
    DI_CHECK_ALLOC_GOTO(tmp);

    for (i = 0; i < (int)phnum; ++i)
    {
        DIANA_MEMSET(tmp, 0, phentsize);

        DI_CHECK_GOTO(DianaExactRead(&pStream->parent, tmp, phentsize));

        // Decode into normalized DIANA_ELF_PROGRAM_HEADER
        DI_CHECK_GOTO(DianaElf_DecodePhdrNormalized(&pSegmentHeader[i],
            tmp,
            (int)phentsize,
            pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT,
            pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE));
        pImpl->capturedSegmentCount = i + 1;
    }

cleanup:
    DIANA_FREE(tmp);
    return DI_SUCCESS;
}
static
int Diana_VerifyElfHeader(DIANA_ELF_HEADER* pElfHeader,
    OPERAND_SIZE sizeOfFile)
{
    if (sizeOfFile && sizeOfFile <= sizeof(DIANA_ELF_HEADER))
        return DI_INVALID_INPUT;

    // Check ELF magic number (0x7F 'E' 'L' 'F')
    if (pElfHeader->e_ident[0] != 0x7F ||
        pElfHeader->e_ident[1] != 'E' ||
        pElfHeader->e_ident[2] != 'L' ||
        pElfHeader->e_ident[3] != 'F')
    {
        return DI_INVALID_INPUT;
    }

    // Check class (32-bit or 64-bit)
    if (pElfHeader->e_ident[4] != DIANA_ELFCLASS32 && pElfHeader->e_ident[4] != DIANA_ELFCLASS64)
        return DI_INVALID_INPUT;

    // Check type (executable, shared object, or relocatable)
    if (pElfHeader->e_type != DIANA_ET_EXEC &&
        pElfHeader->e_type != DIANA_ET_DYN &&
        pElfHeader->e_type != DIANA_ET_REL)
    {
        return DI_INVALID_INPUT;
    }

    // Validate program header offset and size
    if (sizeOfFile && pElfHeader->e_phoff >= sizeOfFile)
        return DI_INVALID_INPUT;

    return DI_SUCCESS;
}


static
int Diana_ReadElfHeader(Diana_ElfFile_impl * elfImpl,
    DianaMovableReadStream* pStream,
    DI_CHAR identHeader[DIANA_EI_NIDENT])
{
    DI_UINT8 cls = identHeader[DIANA_EI_CLASS];
    DI_UINT8 data = identHeader[DIANA_EI_DATA];
    DI_UINT8 tmpHeader[64];
    int ehdrSize = 0;
    int is64 = 0;
    int isLE = 0;

    DIANA_ELF_HEADER* elfHeader = &elfImpl->elfHeader;
    if (cls != DIANA_ELFCLASS32 && cls != DIANA_ELFCLASS64)
    {
        return DI_UNSUPPORTED;
    }
    if (data != DIANA_ELFDATA2LSB && data != DIANA_ELFDATA2MSB)
    {
        return DI_UNSUPPORTED;
    }
    is64 = (cls == DIANA_ELFCLASS64);
    isLE = (data == DIANA_ELFDATA2LSB);

    if (isLE)
    {
        elfImpl->internalFlags |= DIANA_ELF_INTERNAL_FLAG_LE;
    }

    // Check that full ELF header is present
    ehdrSize = is64 ? 64u : 52u;

    DI_CHECK(pStream->pMoveTo(pStream, 0));

    DI_CHECK(DianaExactRead(&pStream->parent, &tmpHeader, ehdrSize));

    DIANA_MEMCPY(elfHeader->e_ident, identHeader, DIANA_EI_NIDENT);

    elfHeader->e_type = DianaElf_rd16(tmpHeader + 0x10, isLE);
    elfHeader->e_machine = DianaElf_rd16(tmpHeader + 0x12, isLE);
    elfHeader->e_version = DianaElf_rd32(tmpHeader + 0x14, isLE);

    if (is64)
    {
        elfImpl->internalFlags |= DIANA_ELF_INTERNAL_FLAG_64BIT;
        elfHeader->e_entry = DianaElf_rd64(tmpHeader + 0x18, isLE);
        elfHeader->e_phoff = DianaElf_rd64(tmpHeader + 0x20, isLE);
        elfHeader->e_shoff = DianaElf_rd64(tmpHeader + 0x28, isLE);
        elfHeader->e_flags = DianaElf_rd32(tmpHeader + 0x30, isLE);
        elfHeader->e_ehsize = DianaElf_rd16(tmpHeader + 0x34, isLE);
        elfHeader->e_phentsize = DianaElf_rd16(tmpHeader + 0x36, isLE);
        elfHeader->e_phnum = DianaElf_rd16(tmpHeader + 0x38, isLE);
        elfHeader->e_shentsize = DianaElf_rd16(tmpHeader + 0x3A, isLE);
        elfHeader->e_shnum = DianaElf_rd16(tmpHeader + 0x3C, isLE);
        elfHeader->e_shstrndx = DianaElf_rd16(tmpHeader + 0x3E, isLE);
    }
    else
    {
        // ELF32: e_entry/e_phoff/e_shoff are 32-bit
        elfHeader->e_entry = (DI_UINT64)DianaElf_rd32(tmpHeader + 0x18, isLE);
        elfHeader->e_phoff = (DI_UINT64)DianaElf_rd32(tmpHeader + 0x1C, isLE);
        elfHeader->e_shoff = (DI_UINT64)DianaElf_rd32(tmpHeader + 0x20, isLE);
        elfHeader->e_flags = DianaElf_rd32(tmpHeader + 0x24, isLE);
        elfHeader->e_ehsize = DianaElf_rd16(tmpHeader + 0x28, isLE);
        elfHeader->e_phentsize = DianaElf_rd16(tmpHeader + 0x2A, isLE);
        elfHeader->e_phnum = DianaElf_rd16(tmpHeader + 0x2C, isLE);
        elfHeader->e_shentsize = DianaElf_rd16(tmpHeader + 0x2E, isLE);
        elfHeader->e_shnum = DianaElf_rd16(tmpHeader + 0x30, isLE);
        elfHeader->e_shstrndx = DianaElf_rd16(tmpHeader + 0x32, isLE);
    }

    return DI_SUCCESS;
}

static
int Diana_InitElfFileImpl(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    OPERAND_SIZE sizeOfFile,
    int flags)
{
    DI_CHAR identHeader[DIANA_EI_NIDENT];
    int mode = 0;
    Diana_ElfFile_impl* pImpl = 0;
    int i;
    OPERAND_SIZE maxSize = 0;

    pElfFile->flags = flags;

    // allocate impl structure
    pImpl = DIANA_MALLOC(sizeof(Diana_ElfFile_impl));
    DI_CHECK_ALLOC(pImpl);

    pElfFile->pImpl = pImpl;
    DIANA_MEMSET(pImpl, 0, sizeof(*pImpl));

    // read ELF header
    DI_CHECK(pStream->pMoveTo(pStream, 0));
    DI_CHECK(DianaExactRead(&pStream->parent, identHeader, sizeof(identHeader)));
    
    DI_CHECK(Diana_ReadElfHeader(pImpl, pStream, identHeader));

    DI_CHECK(Diana_VerifyElfHeader(&pImpl->elfHeader, sizeOfFile));

    // Determine mode based on ELF class
    if (identHeader[DIANA_EI_CLASS] == DIANA_ELFCLASS32)
        mode = DIANA_MODE32;
    else if (identHeader[DIANA_EI_CLASS] == DIANA_ELFCLASS64)
        mode = DIANA_MODE64;
    else
        return DI_UNSUPPORTED_COMMAND;

    // Read program headers
    DI_CHECK(ReadProgramHeaders(pImpl, pStream, sizeOfFile));

    if (flags & DIANA_ELF_FILE_FLAGS_FILE_MODE) {
        // Read section headers
        DI_CHECK(ReadSectionHeaders(pImpl, pStream, sizeOfFile));

        // Initialize symbol tables
        DI_CHECK(InitializeSymbolTables(pImpl));
    }

    pElfFile->dianaMode = mode;

    // Calculate module size based on segments
    for (i = 0; i < pImpl->capturedSegmentCount; ++i)
    {
        DIANA_ELF_PROGRAM_HEADER* pHeader = pImpl->pCapturedSegments + i;
        OPERAND_SIZE segmentEnd = pHeader->p_vaddr + pHeader->p_memsz;

        if (pHeader->p_type == DIANA_PT_DYNAMIC)
        {
            pImpl->dynamicAddress = pHeader->p_vaddr;
            pImpl->dynamicSize = pHeader->p_memsz;
        }
        if (segmentEnd > maxSize)
            maxSize = segmentEnd;
    }

    pImpl->sizeOfModule = maxSize;

    return DI_SUCCESS;
}

int DianaElfFile_Init(/* out */ Diana_ElfFile* pElfFile,
    /* in */ DianaMovableReadStream* pStream,
    /* in, optional */ OPERAND_SIZE sizeOfFile,
    /* in*/ int flags)
{
    int status = Diana_InitElfFileImpl(pElfFile, pStream, sizeOfFile, flags);

    if (status)
    {
        DianaElfFile_Free(pElfFile);
    }

    return status;
}

void DianaElfFile_Free(Diana_ElfFile* pElfFile)
{
    if (pElfFile->pImpl)
    {
        if (pElfFile->pImpl->pCapturedSegments)
        {
            DIANA_FREE(pElfFile->pImpl->pCapturedSegments);
            pElfFile->pImpl->pCapturedSegments = 0;
        }

        if (pElfFile->pImpl->pCapturedSections)
        {
            DIANA_FREE(pElfFile->pImpl->pCapturedSections);
            pElfFile->pImpl->pCapturedSections = 0;
        }

        DIANA_FREE(pElfFile->pImpl);
        pElfFile->pImpl = 0;
    }
}



#define DI_ELF_DT_NULL      0
#define DI_ELF_DT_NEEDED    1
#define DI_ELF_DT_STRTAB    5
#define DI_ELF_DT_SYMTAB    6
#define DI_ELF_DT_HASH      4
#define DI_ELF_DT_GNU_HASH  0x6ffffef5
#define DI_ELF_DT_NULL        0
#define DI_ELF_DT_NEEDED      1
#define DI_ELF_DT_PLTRELSZ    2
#define DI_ELF_DT_PLTGOT      3
#define DI_ELF_DT_HASH        4
#define DI_ELF_DT_STRTAB      5
#define DI_ELF_DT_SYMTAB      6
#define DI_ELF_DT_RELA        7
#define DI_ELF_DT_RELASZ      8
#define DI_ELF_DT_RELAENT     9
#define DI_ELF_DT_STRSZ       10
#define DI_ELF_DT_SYMENT      11
#define DI_ELF_DT_INIT        12
#define DI_ELF_DT_FINI        13
#define DI_ELF_DT_SONAME      14
#define DI_ELF_DT_RPATH       15
#define DI_ELF_DT_SYMBOLIC    16
#define DI_ELF_DT_REL         17
#define DI_ELF_DT_RELSZ       18
#define DI_ELF_DT_RELENT      19
#define DI_ELF_DT_PLTREL      20
#define DI_ELF_DT_DEBUG       21
#define DI_ELF_DT_TEXTREL     22
#define DI_ELF_DT_JMPREL      23
#define DI_ELF_DT_BIND_NOW    24
#define DI_ELF_DT_INIT_ARRAY  25
#define DI_ELF_DT_FINI_ARRAY  26
#define DI_ELF_DT_INIT_ARRAYSZ 27
#define DI_ELF_DT_FINI_ARRAYSZ 28


typedef struct _DI_ELF_DYN64
{
    DI_INT64 d_tag;
    DI_UINT64 d_val; // union {ptr,val} normalized as 64bit
} DI_ELF_DYN64;

typedef struct _DI_ELF_SYM64
{
    DI_UINT32 st_name;
    DI_UINT8  st_info;
    DI_UINT8  st_other;
    DI_UINT16 st_shndx;
    DI_UINT64 st_value;
    DI_UINT64 st_size;
} DI_ELF_SYM64;

#define DI_ELF_ST_BIND(i)   ((i) >> 4)
#define DI_ELF_ST_TYPE(i)   ((i) & 0x0f)


static DI_UINT32 DianaElf_SysVHash(const unsigned char* s)
{
    DI_UINT32 h = 0, g;
    while (*s)
    {
        h = (h << 4) + *s++;
        g = h & 0xf0000000U;
        if (g)
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

static DI_UINT32 DianaElf_GnuHash(const unsigned char* s)
{
    DI_UINT32 h = 5381;
    for (; *s; ++s)
        h = h * 33 + *s;
    return h;
}

static int DianaElf_ReadSymNormalized(DianaMovableReadStream* pStream,
    OPERAND_SIZE symEntryAddr,
    int is64bit,
    int isLE,
    DI_ELF_SYM64* pSym,
    int streamFlags)
{
    OPERAND_SIZE readBytes = 0;

    if (is64bit)
    {
        unsigned char buf[24]; // Elf64_Sym is 24 bytes
        DI_CHECK(pStream->pRandomRead(pStream,
            symEntryAddr,
            buf,
            sizeof(buf),
            &readBytes,
            streamFlags));
        if (readBytes != sizeof(buf))
            return DI_ERROR;

        pSym->st_name = DianaElf_rd32(buf + 0x00, isLE);
        pSym->st_info = buf[0x04];
        pSym->st_other = buf[0x05];
        pSym->st_shndx = DianaElf_rd16(buf + 0x06, isLE);
        pSym->st_value = DianaElf_rd64(buf + 0x08, isLE);
        pSym->st_size = DianaElf_rd64(buf + 0x10, isLE);
    }
    else
    {
        unsigned char buf[16]; // Elf32_Sym is 16 bytes
        DI_CHECK(pStream->pRandomRead(pStream,
            symEntryAddr,
            buf,
            sizeof(buf),
            &readBytes,
            streamFlags));
        if (readBytes != sizeof(buf))
            return DI_ERROR;

        pSym->st_name = DianaElf_rd32(buf + 0x00, isLE);
        pSym->st_value = (DI_UINT64)DianaElf_rd32(buf + 0x04, isLE);
        pSym->st_size = (DI_UINT64)DianaElf_rd32(buf + 0x08, isLE);
        pSym->st_info = buf[0x0C];
        pSym->st_other = buf[0x0D];
        pSym->st_shndx = DianaElf_rd16(buf + 0x0E, isLE);
    }
    return DI_SUCCESS;
}
static
int DianaElfFile_GetSymbolAddress_Memory(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    const char* symbolName,
    OPERAND_SIZE* pSymbolAddress)
{
    char nameBuf[256];
    DI_UINT32* buckets = 0;
    DI_UINT64* bloom = 0;
    int status = DI_ERROR;
    DI_ELF_DYN64* pDyn = 0;
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;
    OPERAND_SIZE readBytes = 0;
    OPERAND_SIZE dynAddr = 0;
    DI_UINT64 dynSize = 0;
    int is64bit = 0;
    int isLE = 0;
    OPERAND_SIZE symtabAddr = 0;
    OPERAND_SIZE strtabAddr = 0;
    OPERAND_SIZE gnuHashAddr = 0;
    OPERAND_SIZE hashAddr = 0;
    DI_ELF_DYN64* pEntry = 0;
    DI_ELF_DYN64* pDynEnd = 0;

    if (!pImpl || !symbolName || !pSymbolAddress)
        return DI_INVALID_INPUT;

    *pSymbolAddress = 0;

    is64bit = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
    isLE = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;

    // Dynamic array is at baseAddress + p_impl->dynamicAddress
    dynAddr = pImpl->dynamicAddress;

    dynSize = pImpl->dynamicSize;
    if (dynSize == 0 || dynSize > DIANA_MAX_SAFE_ALLOC_SIZE) // sanity
        return DI_INVALID_INPUT;

    // Read dynamic array into temp (still treated as Elf64_Dyn-like,
    // but d_tag/d_val are read via rdXX in your initializer)
    {
        DIANA_SIZE_T dynAllocSize;
        DI_CHECK_GOTO(Diana_ConvertOpSizeToSizeT(&dynSize, &dynAllocSize));
        pDyn = (DI_ELF_DYN64*)DIANA_MALLOC(dynAllocSize);
        DI_CHECK_ALLOC_GOTO(pDyn);
    }

    DI_CHECK_GOTO(pStream->pRandomRead(pStream,
        dynAddr,
        pDyn,
        (int)dynSize,
        &readBytes,
        0));
    if (readBytes < sizeof(DI_ELF_DYN64))
    {
        DI_CHECK_GOTO(DI_ERROR);
    }

    // Walk dynamic entries
    pEntry = pDyn;
    pDynEnd = (DI_ELF_DYN64*)((DI_UINT8*)pDyn + readBytes);

    for (; pEntry < pDynEnd && pEntry->d_tag != DI_ELF_DT_NULL; ++pEntry)
    {
        switch (pEntry->d_tag)
        {
        case DI_ELF_DT_SYMTAB:
            symtabAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_STRTAB:
            strtabAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_GNU_HASH:
            gnuHashAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_HASH:
            if (!gnuHashAddr)
                hashAddr = pEntry->d_val;
            break;
        default:
            break;
        }
    }

    if (!symtabAddr || !strtabAddr)
    {
        DI_CHECK_GOTO(DI_NOT_FOUND);
    }

    // ================= GNU HASH =================
    // All addresses below (gnuHashAddr, bloomAddr, bucketsAddr, chainBaseAddr,
    // symtabAddr, strtabAddr) are ELF virtual addresses from DT_* entries —
    // they need DIANA_ANALYZE_RANDOM_READ_ABSOLUTE so that translateAbsoluteAddress
    // can convert them to buffer offsets (subtracts m_imageBase).
    if (gnuHashAddr)
    {
        unsigned char hdrRaw[16]; // 4x uint32
        DI_UINT32 header[4];
        DI_UINT32 nbuckets = 0;
        DI_UINT32 symoffset = 0;
        DI_UINT32 bloomsize = 0;
        OPERAND_SIZE bloomAddr = 0;
        OPERAND_SIZE bucketsAddr = 0;
        OPERAND_SIZE chainBaseAddr = 0;
        DI_UINT32 h = 0;
        // Bloom check
        DI_UINT64 bloomWord = 0;
        DI_UINT64 mask = 0;
        DI_UINT32 idx = 0;

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            gnuHashAddr,
            hdrRaw,
            sizeof(hdrRaw),
            &readBytes,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (readBytes != sizeof(hdrRaw))
        {
            DI_CHECK_GOTO(DI_ERROR);
        }

        header[0] = DianaElf_rd32(hdrRaw + 0, isLE);
        header[1] = DianaElf_rd32(hdrRaw + 4, isLE);
        header[2] = DianaElf_rd32(hdrRaw + 8, isLE);
        header[3] = DianaElf_rd32(hdrRaw + 12, isLE);

        nbuckets = header[0];
        symoffset = header[1];
        bloomsize = header[2];
        // DI_UINT32 bloomshift = header[3]; // not needed in this impl

        bloomAddr = gnuHashAddr + sizeof(header);
        if ((OPERAND_SIZE)bloomsize * sizeof(DI_UINT64) > DIANA_MAX_SAFE_ALLOC_SIZE) {
            DI_CHECK_GOTO(DI_OUT_OF_MEMORY);
        }
        bloom = (DI_UINT64*)DIANA_MALLOC((DIANA_SIZE_T)(bloomsize * sizeof(DI_UINT64)));
        DI_CHECK_ALLOC_GOTO(bloom);

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            bloomAddr,
            bloom,
            bloomsize * sizeof(DI_UINT64),
            &readBytes,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (readBytes != bloomsize * sizeof(DI_UINT64))
        {
            DI_CHECK_GOTO(DI_ERROR);
        }

        bucketsAddr = bloomAddr + bloomsize * sizeof(DI_UINT64);
        if ((OPERAND_SIZE)nbuckets * sizeof(DI_UINT32) > DIANA_MAX_SAFE_ALLOC_SIZE) {
            DI_CHECK_GOTO(DI_INVALID_INPUT);
        }
        buckets = (DI_UINT32*)DIANA_MALLOC((DIANA_SIZE_T)(nbuckets * sizeof(DI_UINT32)));
        DI_CHECK_ALLOC_GOTO(buckets);

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            bucketsAddr,
            buckets,
            nbuckets * sizeof(DI_UINT32),
            &readBytes,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (readBytes != nbuckets * sizeof(DI_UINT32))
        {
            DI_CHECK_GOTO(DI_ERROR);
        }

        chainBaseAddr = bucketsAddr + nbuckets * sizeof(DI_UINT32);

        h = DianaElf_GnuHash((const unsigned char*)symbolName);

        // Bloom check
        bloomWord = bloom[(h / 64) % bloomsize];
        mask = (DI_UINT64)1 << (h % 64);
        if (!(bloomWord & mask))
        {
            DI_CHECK_GOTO(DI_NOT_FOUND);
        }

        idx = buckets[h % nbuckets];
        if (idx < symoffset)
        {
            DI_CHECK_GOTO(DI_NOT_FOUND);
        }

        // Chain walk
        for (;; ++idx)
        {
            DI_UINT32 chainValLE;
            DI_UINT32 chainVal;
            OPERAND_SIZE chainEntryAddr = chainBaseAddr + (OPERAND_SIZE)idx * sizeof(DI_UINT32);
            DI_ELF_SYM64 sym;
            OPERAND_SIZE symEntryAddr;
            OPERAND_SIZE nameAddr;

            DI_CHECK_GOTO(pStream->pRandomRead(pStream,
                chainEntryAddr,
                &chainValLE,
                sizeof(chainValLE),
                &readBytes,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
            if (readBytes != sizeof(chainValLE))
            {
                DI_CHECK_GOTO(DI_ERROR);
            }
            chainVal = DianaElf_rd32((unsigned char*)&chainValLE, isLE);

            symEntryAddr = symtabAddr + (OPERAND_SIZE)idx *
                (is64bit ? sizeof(DI_ELF_SYM64) : 16 /* Elf32_Sym */);

            DI_CHECK_GOTO(DianaElf_ReadSymNormalized(pStream,
                symEntryAddr,
                is64bit,
                isLE,
                &sym,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));

            nameAddr = strtabAddr + sym.st_name;
            DI_CHECK_GOTO(pStream->pRandomRead(pStream,
                nameAddr,
                nameBuf,
                sizeof(nameBuf) - 1,
                &readBytes,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
            if (readBytes == 0)
            {
                DI_CHECK_GOTO(DI_ERROR);
            }
            nameBuf[sizeof(nameBuf) - 1] = 0;

            if (DIANA_STRNCMP(nameBuf, symbolName, sizeof(nameBuf)) == 0)
            {
                int bind = DI_ELF_ST_BIND(sym.st_info);
                int type = DI_ELF_ST_TYPE(sym.st_info);

                if ((bind == DIANA_STB_GLOBAL || bind == DIANA_STB_WEAK) &&
                    (type == DIANA_STT_FUNC || type == DIANA_STT_OBJECT) &&
                    sym.st_value != 0)
                {
                    *pSymbolAddress = sym.st_value;
                    status = DI_SUCCESS;
                    goto cleanup;
                }
            }

            if (chainVal & 1)
                break;
        }

        if (buckets)
        {
            DIANA_FREE(buckets);
            buckets = 0;
        }
        if (bloom)
        {
            DIANA_FREE(bloom);
            bloom = 0;
        }
    }

    // =============== SysV HASH fallback ===============
    if (hashAddr)
    {
        unsigned char hdrRaw[8];
        DI_UINT32 nbuckets, nchains;
        OPERAND_SIZE bucketsAddr = 0;
        OPERAND_SIZE chainsAddr = 0;
        DI_UINT32 h = 0;
        DI_UINT32 idx = 0;

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            hashAddr,
            hdrRaw,
            sizeof(hdrRaw),
            &readBytes,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (readBytes != sizeof(hdrRaw))
        {
            DI_CHECK_GOTO(DI_ERROR);
        }

        nbuckets = DianaElf_rd32(hdrRaw + 0, isLE);
        nchains = DianaElf_rd32(hdrRaw + 4, isLE);

        bucketsAddr = hashAddr + sizeof(hdrRaw);
        chainsAddr = bucketsAddr + nbuckets * sizeof(DI_UINT32);

        buckets = (DI_UINT32*)DIANA_MALLOC(nbuckets * sizeof(DI_UINT32));
        DI_CHECK_ALLOC_GOTO(buckets);

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            bucketsAddr,
            buckets,
            nbuckets * sizeof(DI_UINT32),
            &readBytes,
            DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (readBytes != nbuckets * sizeof(DI_UINT32))
        {
            DI_CHECK_GOTO(DI_ERROR);
        }

        h = DianaElf_SysVHash((const unsigned char*)symbolName);
        idx = buckets[h % nbuckets];

        while (idx != DIANA_STN_UNDEF && idx < nchains)
        {
            DI_ELF_SYM64 sym;
            OPERAND_SIZE symEntryAddr = symtabAddr + (OPERAND_SIZE)idx *
                (is64bit ? sizeof(DI_ELF_SYM64) : 16);

            OPERAND_SIZE nameAddr = 0;
            DI_UINT32 nextIdxRaw = 0;
            OPERAND_SIZE chainEntryAddr = 0;

            DI_CHECK_GOTO(DianaElf_ReadSymNormalized(pStream,
                symEntryAddr,
                is64bit,
                isLE,
                &sym,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));

            nameAddr = strtabAddr + sym.st_name;
            DI_CHECK_GOTO(pStream->pRandomRead(pStream,
                nameAddr,
                nameBuf,
                sizeof(nameBuf) - 1,
                &readBytes,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
            if (readBytes == 0)
            {
                DI_CHECK_GOTO(DI_ERROR);
            }
            nameBuf[sizeof(nameBuf) - 1] = 0;

            if (DIANA_STRNCMP(nameBuf, symbolName, sizeof(nameBuf)) == 0)
            {
                int bind = DI_ELF_ST_BIND(sym.st_info);
                int type = DI_ELF_ST_TYPE(sym.st_info);

                if ((bind == DIANA_STB_GLOBAL || bind == DIANA_STB_WEAK) &&
                    (type == DIANA_STT_FUNC || type == DIANA_STT_OBJECT) &&
                    sym.st_value != 0)
                {
                    *pSymbolAddress = sym.st_value;
                    status = DI_SUCCESS;
                    goto cleanup;
                }
            }

            chainEntryAddr = chainsAddr + (OPERAND_SIZE)idx * sizeof(DI_UINT32);
            DI_CHECK_GOTO(pStream->pRandomRead(pStream,
                chainEntryAddr,
                &nextIdxRaw,
                sizeof(nextIdxRaw),
                &readBytes,
                DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
            if (readBytes != sizeof(nextIdxRaw))
            {
                DI_CHECK_GOTO(DI_ERROR);
            }

            idx = DianaElf_rd32((unsigned char*)&nextIdxRaw, isLE);
        }
    }

cleanup:
    if (bloom)
    {
        DIANA_FREE(bloom);
    }
    if (buckets)
    {
        DIANA_FREE(buckets);
    }
    if (pDyn)
    {
        DIANA_FREE(pDyn);
    }
    return status;
}


static
int DianaElfFile_GetSymbolAddress_File(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    const char* symbolName,
    OPERAND_SIZE* pSymbolAddress)
{
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;
    if (!pImpl || !symbolName || !pSymbolAddress)
        return DI_INVALID_INPUT;

    *pSymbolAddress = 0;

    {
        const int is64bit = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
        const int isLE = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;


        // Get dynamic symbol / string table sections
        DIANA_ELF_SECTION_HEADER* pDynsym = &pImpl->pDynamicSymbolTableSection->header;
        DIANA_ELF_SECTION_HEADER* pDynstr = &pImpl->pDynamicStringTableSection->header;
        DI_UINT64 symEntSize = 0;
        int status = DI_NOT_FOUND;
        char* strings = NULL;
        DI_UINT64 numSymbols = 0;
        DI_UINT64 i = 0;

        if (!pDynsym || !pDynstr)
            return DI_ERROR;  // No dynamic symbols

        // Number of entries depends on class
        symEntSize = is64bit ? 24 : 16;
        if (pDynsym->sh_entsize != 0)
            symEntSize = pDynsym->sh_entsize; // respect file if set

        if (symEntSize == 0)
            return DI_INVALID_INPUT;

        numSymbols = pDynsym->sh_size / symEntSize;

        if (pDynstr->sh_size == 0 || pDynstr->sh_size > DIANA_MAX_SAFE_ALLOC_SIZE)
            return DI_INVALID_INPUT;

        // Read entire string table
        {
            OPERAND_SIZE dynstrOpSize = pDynstr->sh_size;
            DIANA_SIZE_T dynstrAllocSize;
            DI_CHECK_GOTO(Diana_ConvertOpSizeToSizeT(&dynstrOpSize, &dynstrAllocSize));
            strings = (char*)DIANA_MALLOC(dynstrAllocSize);
            DI_CHECK_ALLOC_GOTO(strings);
        }

        DI_CHECK_GOTO(pStream->pMoveTo(pStream, pDynstr->sh_offset));
        DI_CHECK_GOTO(DianaExactReadSafe(&pStream->parent, strings, pDynstr->sh_size));

        // Linear search over symbols (normalized per entry)
        for (i = 0; i < numSymbols; ++i)
        {
            DI_ELF_SYM64 sym;
            const char* name = 0;

            OPERAND_SIZE symOffset = pDynsym->sh_offset + (OPERAND_SIZE)(i * symEntSize);

            DI_CHECK_GOTO(DianaElf_ReadSymNormalized(pStream,
                symOffset,
                is64bit,
                isLE,
                &sym,
                0));

            if (sym.st_name >= pDynstr->sh_size)
                continue;

            name = &strings[sym.st_name];

            if (DIANA_STRNCMP(name, symbolName, 255) == 0)
            {
                int binding = DI_ELF_ST_BIND(sym.st_info);
                int type = DI_ELF_ST_TYPE(sym.st_info);

                if ((binding == DIANA_STB_GLOBAL || binding == DIANA_STB_WEAK) &&
                    (type == DIANA_STT_FUNC || type == DIANA_STT_OBJECT))
                {
                    *pSymbolAddress = sym.st_value;
                    status = DI_SUCCESS;
                    break;
                }
            }
        }
    cleanup:

        if (strings)
        {
            DIANA_FREE(strings);
        }
        return status;
    }
}

int DianaElfFile_GetProcAddress(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    const char* symbolName,
    OPERAND_SIZE* pSymbolAddress)
{
    if (pElfFile->flags & DIANA_ELF_FILE_FLAGS_FILE_MODE)
    {
        return DianaElfFile_GetSymbolAddress_File(pElfFile, pStream, symbolName, pSymbolAddress);
    }
    return DianaElfFile_GetSymbolAddress_Memory(pElfFile, pStream, symbolName, pSymbolAddress);
}

/* Write a 64-bit value to the output stream at VA `dest` */
static int DianaElf_Write64(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE dest,
    DI_UINT64    value,
    int          isLE);

static int DianaElf_Write32(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE dest,
    DI_UINT32    value,
    int          isLE);


static
int DianaElfFile_QueryImportsExports(Diana_ElfFile* pElfFile,
    OPERAND_SIZE baseAddress,
    DianaMovableReadStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags,
    int importFlags,
    int bImports)
{
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;
    OPERAND_SIZE readBytes = 0;
    DI_ELF_DYN64* pDyn = 0;
    int status = DI_ERROR;
    int is64bit = 0;
    int isLE = 0;
    OPERAND_SIZE dynAddr = 0;
    DI_UINT64 dynSize = 0;

    OPERAND_SIZE symtabAddr = 0;
    OPERAND_SIZE strtabAddr = 0;
    OPERAND_SIZE jmprelAddr = 0;
    DI_UINT64    jmprelSize = 0;
    DI_UINT64    relaEntSize = 0;
    DI_UINT64    pltRelType = 0; // DT_PLTREL: REL or RELA
    DI_ELF_DYN64* pEntry = 0;
    DI_ELF_DYN64* pDynEnd = 0;


    if (!pImpl || !pObserver || !pObserver->queryFunctionByName)
        return DI_INVALID_INPUT;

    if (pElfFile->flags & DIANA_ELF_FILE_FLAGS_FILE_MODE)
        return DI_UNSUPPORTED;

    is64bit = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
    isLE = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;

    // 1) Read dynamic section
    dynAddr = baseAddress;
    DI_CHECK_GOTO(Diana_SafeAdd(&dynAddr, pImpl->dynamicAddress));

    dynSize = pImpl->dynamicSize;
    if (dynSize == 0 || dynSize > DIANA_MAX_SAFE_ALLOC_SIZE)
        return DI_INVALID_INPUT;

    {
        DIANA_SIZE_T dynAllocSize;
        DI_CHECK_GOTO(Diana_ConvertOpSizeToSizeT(&dynSize, &dynAllocSize));
        pDyn = (DI_ELF_DYN64*)DIANA_MALLOC(dynAllocSize);
        DI_CHECK_ALLOC_GOTO(pDyn);
    }

    DI_CHECK_GOTO(pOutStream->pRandomRead(pOutStream,
        dynAddr,
        pDyn,
        (int)dynSize,
        &readBytes,
        streamFlags));
    if (readBytes < sizeof(DI_ELF_DYN64))
        DI_CHECK_GOTO(DI_ERROR);

    // 2) Parse dynamic entries
    symtabAddr = 0;
    strtabAddr = 0;
    jmprelAddr = 0;
    jmprelSize = 0;
    relaEntSize = 0;
    pltRelType = 0; // DT_PLTREL: REL or RELA

    pEntry = pDyn;
    pDynEnd = (DI_ELF_DYN64*)((DI_UINT8*)pDyn + readBytes);

    for (; pEntry < pDynEnd && pEntry->d_tag != DI_ELF_DT_NULL; ++pEntry)
    {
        switch (pEntry->d_tag)
        {
        case DI_ELF_DT_SYMTAB:
            symtabAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_STRTAB:
            strtabAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_JMPREL:
            jmprelAddr = pEntry->d_val;
            break;
        case DI_ELF_DT_PLTRELSZ:
            jmprelSize = pEntry->d_val;
            break;
        case DI_ELF_DT_RELAENT:
        case DI_ELF_DT_RELENT:
            relaEntSize = pEntry->d_val;
            break;
        case DI_ELF_DT_PLTREL:
            pltRelType = pEntry->d_val; // DT_REL or DT_RELA
            break;
        default:
            break;
        }
    }

    if (!symtabAddr || !strtabAddr || !jmprelAddr || !jmprelSize)
    {
        status = DI_SUCCESS; // nothing to report
        goto cleanup;
    }

    if (relaEntSize == 0)
    {
        if (is64bit)
            relaEntSize = (pltRelType == DI_ELF_DT_RELA) ? 24 : 16;
        else
            relaEntSize = (pltRelType == DI_ELF_DT_RELA) ? 12 : 8;
    }

    // 3) Enumerate PLT relocations
    {
        DI_UINT64 count = jmprelSize / relaEntSize;
        DI_UINT64 idx;
        DI_ELF_SYM64 sym;

        for (idx = 0; idx < count; ++idx)
        {
            unsigned char relBuf[32];
            DI_UINT64 thisRelOffset = jmprelAddr + idx * relaEntSize;
            DI_UINT64 r_offset = 0;
            DI_UINT64 r_info = 0;
            DI_UINT32 symIndex;
            OPERAND_SIZE symEntryAddr = 0;

            DI_CHECK_GOTO(pOutStream->pRandomRead(pOutStream,
                thisRelOffset,
                relBuf,
                (int)relaEntSize,
                &readBytes,
                streamFlags | DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
            if (readBytes != relaEntSize)
                DI_CHECK_GOTO(DI_ERROR);


            if (pltRelType == DI_ELF_DT_RELA)
            {
                if (is64bit)
                {
                    r_offset = DianaElf_rd64(relBuf + 0x00, isLE);
                    r_info = DianaElf_rd64(relBuf + 0x08, isLE);
                }
                else
                {
                    r_offset = (DI_UINT64)DianaElf_rd32(relBuf + 0x00, isLE);
                    r_info = (DI_UINT64)DianaElf_rd32(relBuf + 0x04, isLE);
                }
            }
            else // DT_REL
            {
                if (is64bit)
                {
                    r_offset = DianaElf_rd64(relBuf + 0x00, isLE);
                    r_info = DianaElf_rd64(relBuf + 0x08, isLE);
                }
                else
                {
                    r_offset = (DI_UINT64)DianaElf_rd32(relBuf + 0x00, isLE);
                    r_info = (DI_UINT64)DianaElf_rd32(relBuf + 0x04, isLE);
                }
            }

            if (is64bit)
                symIndex = (DI_UINT32)(r_info >> 32); // ELF64_R_SYM
            else
                symIndex = (DI_UINT32)(r_info >> 8);   // ELF32_R_SYM

            symEntryAddr = symtabAddr +
                (OPERAND_SIZE)symIndex *
                (is64bit ? 24 : 16);

            DI_CHECK_GOTO(DianaElf_ReadSymNormalized(pOutStream,
                symEntryAddr,
                is64bit,
                isLE,
                &sym,
                streamFlags | DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));

            if (bImports) 
            {
                if (sym.st_shndx != 0)
                    continue;
            }
            else 
            {
                if (sym.st_shndx == 0)
                    continue;
            }

            {
                char funcName[256];
                OPERAND_SIZE symNameAddr = 0;
                const char* dllName = 0;
                OPERAND_SIZE stValue = 0;
                OPERAND_SIZE gotSlotAddr = 0;
                int bind = DI_ELF_ST_BIND(sym.st_info);
                int type = DI_ELF_ST_TYPE(sym.st_info);
                if ((bind != DIANA_STB_GLOBAL && bind != DIANA_STB_WEAK) ||
                    (type != DIANA_STT_FUNC && type != DIANA_STT_OBJECT))
                {
                    continue;
                }

                symNameAddr = strtabAddr + sym.st_name;
                if (pObserver->reportInfo_fnc)
                {
                    DI_CHECK_GOTO(pObserver->reportInfo_fnc(pObserver, symNameAddr));
                }
                DI_CHECK_GOTO(pOutStream->pRandomRead(pOutStream,
                    symNameAddr,
                    funcName,
                    sizeof(funcName) - 1,
                    &readBytes,
                    streamFlags | DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
                if (!readBytes)
                    DI_CHECK_GOTO(DI_ERROR);
                funcName[sizeof(funcName) - 1] = 0;

                stValue = sym.st_value;

                /* GOT slot address for write-back (only used when stValue==0, i.e. external import) */
                gotSlotAddr = 0;
                if (!stValue && bImports)
                {
                    gotSlotAddr = r_offset;
                    if (pElfFile->pImpl->elfHeader.e_type == DIANA_ET_DYN)
                        DI_CHECK_GOTO(Diana_SafeAdd(&gotSlotAddr, baseAddress));
                }

                DI_CHECK_GOTO(pObserver->queryFunctionByName(
                    pObserver,
                    dllName,
                    funcName,
                    0,
                    &stValue));

                /* Write resolved address back to the GOT slot, mirroring what
                   DianaPeFile_LinkImports does for IAT slots. Only applicable
                   when the GOT was unresolved (file-mode image). */
                if (gotSlotAddr && stValue)
                {
                    DianaReadWriteRandomStream* pRwStream = (DianaReadWriteRandomStream*)pOutStream;
                    if (pRwStream->pRandomWrite)
                    {
                        if (is64bit)
                        { 
                            DI_CHECK_GOTO(DianaElf_Write64(pRwStream, gotSlotAddr, stValue, isLE)); 
                        }
                        else
                        { 
                            DI_CHECK_GOTO(DianaElf_Write32(pRwStream, gotSlotAddr, (DI_UINT32)stValue, isLE)); 
                        }
                    }
                }
            }
        }
    }

    status = DI_SUCCESS;

cleanup:
    if (pDyn)
        DIANA_FREE(pDyn);
    return status;
}


int DianaElfFile_QueryExports(/* in */ Diana_ElfFile* pElfFile,
    /* inout */ DianaMovableReadStream* pOutStream,
    /* in */ void* pPage,
    /* in */ int pageSize,
    /* in */ DianaPeFile_LinkImports_Observer* pObserver,
    /* in */ int streamFlags)
{
    return DianaElfFile_QueryImportsExports(pElfFile,
        0,
        pOutStream,
        pPage,
        pageSize,
        pObserver,
        streamFlags,
        0,
        0);
}
int DianaElfFile_GetNeededLibraries(Diana_ElfFile* pElfFile,
    DianaMovableReadStream* pStream,
    DianaElfFile_NeededLibrary_Callback callback,
    void* pContext,
    int streamFlags)
{
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;
    DI_ELF_DYN64* pDyn = 0;
    int status = DI_ERROR;
    OPERAND_SIZE readBytes = 0;
    DI_ELF_DYN64* pEntry = 0;
    DI_ELF_DYN64* pDynEnd = 0;
    OPERAND_SIZE strtabAddr = 0;

    if (!pImpl || !callback)
        return DI_INVALID_INPUT;

    if (pImpl->dynamicSize == 0 || pImpl->dynamicSize > DIANA_MAX_SAFE_ALLOC_SIZE)
        return DI_SUCCESS;

    {
        DIANA_SIZE_T dynAllocSize;
        DI_CHECK_GOTO(Diana_ConvertOpSizeToSizeT(&pImpl->dynamicSize, &dynAllocSize));
        pDyn = (DI_ELF_DYN64*)DIANA_MALLOC(dynAllocSize);
        DI_CHECK_ALLOC_GOTO(pDyn);
    }

    DI_CHECK_GOTO(pStream->pRandomRead(pStream,
        pImpl->dynamicAddress,
        pDyn,
        (int)pImpl->dynamicSize,
        &readBytes,
        streamFlags));
    if (readBytes < sizeof(DI_ELF_DYN64))
    {
        DI_CHECK_GOTO(DI_ERROR);
    }
    pEntry = pDyn;
    pDynEnd = (DI_ELF_DYN64*)((DI_UINT8*)pDyn + readBytes);

    /* First pass: locate DT_STRTAB */
    strtabAddr = 0;
    for (pEntry = pDyn; pEntry < pDynEnd && pEntry->d_tag != DI_ELF_DT_NULL; ++pEntry)
    {
        if (pEntry->d_tag == DI_ELF_DT_STRTAB)
        {
            strtabAddr = (OPERAND_SIZE)pEntry->d_val;
            break;
        }
    }
    if (!strtabAddr)
    {
        status = DI_SUCCESS;
        goto cleanup;
    }

    /* Second pass: yield each DT_NEEDED string.
       strtabAddr is an ELF virtual address (like DT_SYMTAB/DT_JMPREL in
       QueryImportsExports), so reads must use DIANA_ANALYZE_RANDOM_READ_ABSOLUTE
       so that translateAbsoluteAddress can subtract the image base. */
    for (pEntry = pDyn; pEntry < pDynEnd && pEntry->d_tag != DI_ELF_DT_NULL; ++pEntry)
    {
        char libName[256];
        OPERAND_SIZE nameAddr = 0;
        OPERAND_SIZE nameRead = 0;

        if (pEntry->d_tag != DI_ELF_DT_NEEDED)
            continue;

        nameAddr = strtabAddr + (OPERAND_SIZE)pEntry->d_val;
        nameRead = 0;

        DI_CHECK_GOTO(pStream->pRandomRead(pStream,
            nameAddr,
            libName,
            sizeof(libName) - 1,
            &nameRead,
            streamFlags | DIANA_ANALYZE_RANDOM_READ_ABSOLUTE));
        if (!nameRead)
            continue;
        libName[sizeof(libName) - 1] = 0;
        DI_CHECK_GOTO(callback(pContext, libName));
    }

    status = DI_SUCCESS;

cleanup:
    if (pDyn)
        DIANA_FREE(pDyn);
    return status;
}

int DianaElfFile_QueryImports(Diana_ElfFile* pElfFile,
    OPERAND_SIZE baseAddress,
    DianaReadWriteRandomStream* pOutStream,
    void* pPage,
    int pageSize,
    DianaPeFile_LinkImports_Observer* pObserver,
    int streamFlags,
    int importFlags)
{
    return DianaElfFile_QueryImportsExports(pElfFile,
        baseAddress,
        &pOutStream->parent,
        pPage,
        pageSize,
        pObserver,
        streamFlags,
        importFlags,
        1);
}



// -----------------------------------------------------------------------
// Normalized relocation record (covers both REL and RELA, 32/64-bit)
// -----------------------------------------------------------------------
typedef struct _DianaElf_RelaEntry
{
    OPERAND_SIZE r_offset;  /* where to patch (VA in the mapped image) */
    DI_UINT32    r_sym;     /* symbol table index                       */
    DI_UINT32    r_type;    /* architecture-specific relocation type    */
    DI_INT64     r_addend;  /* explicit addend (0 for REL entries)      */
} DianaElf_RelaEntry;


/* Write a 32-bit value to the output stream at VA `dest` */
static int DianaElf_Write32(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE dest,
    DI_UINT32    value,
    int          isLE)
{
    unsigned char buf[4];
    OPERAND_SIZE written = 0;
    if (isLE)
    {
        buf[0] = (unsigned char)(value);
        buf[1] = (unsigned char)(value >> 8);
        buf[2] = (unsigned char)(value >> 16);
        buf[3] = (unsigned char)(value >> 24);
    }
    else
    {
        buf[0] = (unsigned char)(value >> 24);
        buf[1] = (unsigned char)(value >> 16);
        buf[2] = (unsigned char)(value >> 8);
        buf[3] = (unsigned char)(value);
    }
    return pOut->pRandomWrite(pOut, dest, buf, sizeof(buf), &written, 0);
}

/* Write a 64-bit value to the output stream at VA `dest` */
static int DianaElf_Write64(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE dest,
    DI_UINT64    value,
    int          isLE)
{
    unsigned char buf[8];
    OPERAND_SIZE written = 0;
    if (isLE)
    {
        buf[0] = (unsigned char)(value);
        buf[1] = (unsigned char)(value >> 8);
        buf[2] = (unsigned char)(value >> 16);
        buf[3] = (unsigned char)(value >> 24);
        buf[4] = (unsigned char)(value >> 32);
        buf[5] = (unsigned char)(value >> 40);
        buf[6] = (unsigned char)(value >> 48);
        buf[7] = (unsigned char)(value >> 56);
    }
    else
    {
        buf[0] = (unsigned char)(value >> 56);
        buf[1] = (unsigned char)(value >> 48);
        buf[2] = (unsigned char)(value >> 40);
        buf[3] = (unsigned char)(value >> 32);
        buf[4] = (unsigned char)(value >> 24);
        buf[5] = (unsigned char)(value >> 16);
        buf[6] = (unsigned char)(value >> 8);
        buf[7] = (unsigned char)(value);
    }
    return pOut->pRandomWrite(pOut, dest, buf, sizeof(buf), &written, 0);
}

/*
 * Apply a single relocation entry to the already-mapped output stream.
 *
 * loadBias  = `address` passed to MapEx  (== 0 for ET_EXEC, delta for ET_DYN)
 * pOut      = the mapped memory stream
 * pEntry    = decoded, normalized relocation
 * machine   = e_machine from ELF header
 * is64 / isLE = class / endianness flags
 *
 * Symbol-dependent relocs (r_sym != 0) that are not RELATIVE are
 * SKIPPED here � they are resolved later by DianaElfFile_QueryImports.
 */
static int DianaElf_ApplyReloc(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE                loadBias,
    const DianaElf_RelaEntry* pEntry,
    DI_UINT16                   machine,
    int                         is64,
    int                         isLE)
{
    /* Patch address in the already-loaded image */
    OPERAND_SIZE patchVA = (OPERAND_SIZE)pEntry->r_offset + loadBias;
    OPERAND_SIZE rb = 0;

    switch (machine)
    {
        /* ---- x86-64 ---- */
    case DIANA_EM_X86_64:
    {
        switch (pEntry->r_type)
        {
        case DIANA_R_X86_64_NONE:
            return DI_SUCCESS;

        case DIANA_R_X86_64_RELATIVE:
        {
            /* *patchVA = B + A  (B = loadBias, A = r_addend) */
            DI_UINT64 val = (DI_UINT64)((DI_INT64)loadBias + pEntry->r_addend);
            DI_CHECK(DianaElf_Write64(pOut, patchVA, val, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_X86_64_64:
        {
            unsigned char cur[8];
            DI_UINT64 oldVal = 0;
            DI_UINT64 newVal = 0;

            if (pEntry->r_sym != 0) return DI_SUCCESS; /* needs symbol � skip */
            /* *patchVA += B + A */
            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            oldVal = DianaElf_rd64(cur, isLE);
            newVal = oldVal + (DI_UINT64)loadBias + (DI_UINT64)pEntry->r_addend;
            DI_CHECK(DianaElf_Write64(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_X86_64_32:
        {
            unsigned char cur[4];
            DI_UINT32 oldVal = 0;
            DI_UINT32 newVal = 0;

            if (pEntry->r_sym != 0) return DI_SUCCESS;
            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            oldVal = DianaElf_rd32(cur, isLE);
            newVal = oldVal + (DI_UINT32)loadBias + (DI_UINT32)pEntry->r_addend;
            DI_CHECK(DianaElf_Write32(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_X86_64_GLOB_DAT:
        case DIANA_R_X86_64_JUMP_SLOT:
            /* Symbol-dependent; resolved by QueryImports */
            return DI_SUCCESS;

        default:
            return DI_SUCCESS; /* unknown type � ignore safely */
        }
    }

    /* ---- i386 ---- */
    case DIANA_EM_386:
    {
        switch (pEntry->r_type)
        {
        case DIANA_R_386_NONE:
            return DI_SUCCESS;

        case DIANA_R_386_RELATIVE:
        {
            /* REL style: addend is read from the patch location itself */
            unsigned char cur[4];
            DI_UINT32 implicit = 0;
            DI_INT64  addend = 0;
            DI_UINT32 newVal = 0;

            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            implicit = DianaElf_rd32(cur, isLE);
            addend = pEntry->r_addend != 0
                ? pEntry->r_addend
                : (DI_INT64)(DI_INT32)implicit;
            newVal = (DI_UINT32)((DI_INT64)loadBias + addend);
            DI_CHECK(DianaElf_Write32(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_386_32:
        {
            unsigned char cur[4];
            DI_UINT32 oldVal = 0;
            DI_UINT32 newVal = 0;

            if (pEntry->r_sym != 0) return DI_SUCCESS;
            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            oldVal = DianaElf_rd32(cur, isLE);
            newVal = oldVal + (DI_UINT32)loadBias;
            DI_CHECK(DianaElf_Write32(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_386_GLOB_DAT:
        case DIANA_R_386_JMP_SLOT:
            return DI_SUCCESS;

        default:
            return DI_SUCCESS;
        }
    }

    /* ---- AArch64 ---- */
    case DIANA_EM_AARCH64:
    {
        switch (pEntry->r_type)
        {
        case DIANA_R_AARCH64_NONE:
            return DI_SUCCESS;

        case DIANA_R_AARCH64_RELATIVE:
        {
            DI_UINT64 val = (DI_UINT64)((DI_INT64)loadBias + pEntry->r_addend);
            DI_CHECK(DianaElf_Write64(pOut, patchVA, val, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_AARCH64_ABS64:
        {            
            unsigned char cur[8];
            DI_UINT64 oldVal = 0;
            DI_UINT64 newVal = 0;

            if (pEntry->r_sym != 0) return DI_SUCCESS;
            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            oldVal = DianaElf_rd64(cur, isLE);
            newVal = oldVal + (DI_UINT64)loadBias + (DI_UINT64)pEntry->r_addend;
            DI_CHECK(DianaElf_Write64(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_AARCH64_GLOB_DAT:
        case DIANA_R_AARCH64_JUMP_SLOT:
            return DI_SUCCESS;

        default:
            return DI_SUCCESS;
        }
    }

    /* ---- ARM (32-bit) ---- */
    case DIANA_EM_ARM:
    {
        switch (pEntry->r_type)
        {
        case DIANA_R_ARM_NONE:
            return DI_SUCCESS;

        case DIANA_R_ARM_RELATIVE:
        {
            unsigned char cur[4];
            DI_UINT32 implicit = 0;
            DI_INT64  addend = 0;
            DI_UINT32 newVal = 0;

            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            implicit = DianaElf_rd32(cur, isLE);
            addend = pEntry->r_addend != 0
                ? pEntry->r_addend
                : (DI_INT64)(DI_INT32)implicit;
            newVal = (DI_UINT32)((DI_INT64)loadBias + addend);
            DI_CHECK(DianaElf_Write32(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_ARM_ABS32:
        {
            unsigned char cur[4];
            DI_UINT32 oldVal = 0;
            DI_UINT32 newVal = 0;

            if (pEntry->r_sym != 0) return DI_SUCCESS;
            DI_CHECK(pOut->parent.pRandomRead(pOut, patchVA, cur, sizeof(cur), &rb, 0));
            if (rb != sizeof(cur)) return DI_ERROR;
            oldVal = DianaElf_rd32(cur, isLE);
            newVal = oldVal + (DI_UINT32)loadBias;
            DI_CHECK(DianaElf_Write32(pOut, patchVA, newVal, isLE));
            return DI_SUCCESS;
        }

        case DIANA_R_ARM_GLOB_DAT:
        case DIANA_R_ARM_JUMP_SLOT:
            return DI_SUCCESS;

        default:
            return DI_SUCCESS;
        }
    }

    default:
        return DI_SUCCESS; /* unsupported arch � skip silently */
    }
}


/* Read one REL entry (no addend) from pOutStream at VA `addr` */
static int DianaElf_ReadRel(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE                addr,
    int                         is64,
    int                         isLE,
    DianaElf_RelaEntry* out)
{
    OPERAND_SIZE rb = 0;
    if (is64)
    {
        unsigned char buf[16];
        DI_UINT64 info = 0;
        DI_CHECK(pOut->parent.pRandomRead(pOut, addr, buf, sizeof(buf), &rb, 0));
        if (rb != sizeof(buf)) return DI_ERROR;
        out->r_offset = (OPERAND_SIZE)DianaElf_rd64(buf + 0, isLE);
        info = DianaElf_rd64(buf + 8, isLE);
        out->r_sym = (DI_UINT32)DIANA_ELF_R_SYM(info);
        out->r_type = (DI_UINT32)DIANA_ELF_R_TYPE(info);
        out->r_addend = 0;
    }
    else
    {
        unsigned char buf[8];
        DI_UINT32 info = 0;
        DI_CHECK(pOut->parent.pRandomRead(pOut, addr, buf, sizeof(buf), &rb, 0));
        if (rb != sizeof(buf)) return DI_ERROR;
        out->r_offset = (OPERAND_SIZE)DianaElf_rd32(buf + 0, isLE);
        info = DianaElf_rd32(buf + 4, isLE);
        out->r_sym = (DI_UINT32)DIANA_ELF_R_SYM_32(info);
        out->r_type = (DI_UINT32)DIANA_ELF_R_TYPE_32(info);
        out->r_addend = 0;
    }
    return DI_SUCCESS;
}

/* Read one RELA entry (explicit addend) from pOutStream at VA `addr` */
static int DianaElf_ReadRela(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE                addr,
    int                         is64,
    int                         isLE,
    DianaElf_RelaEntry* out)
{
    OPERAND_SIZE rb = 0;
    if (is64)
    {
        unsigned char buf[24];
        DI_UINT64 info = 0;
        DI_CHECK(pOut->parent.pRandomRead(pOut, addr, buf, sizeof(buf), &rb, 0));
        if (rb != sizeof(buf)) return DI_ERROR;
        out->r_offset = (OPERAND_SIZE)DianaElf_rd64(buf + 0, isLE);
        info = DianaElf_rd64(buf + 8, isLE);
        out->r_sym = (DI_UINT32)DIANA_ELF_R_SYM(info);
        out->r_type = (DI_UINT32)DIANA_ELF_R_TYPE(info);
        out->r_addend = (DI_INT64)DianaElf_rd64(buf + 16, isLE);
    }
    else
    {
        unsigned char buf[12];
        DI_UINT32 info = 0;
        DI_CHECK(pOut->parent.pRandomRead(pOut, addr, buf, sizeof(buf), &rb, 0));
        if (rb != sizeof(buf)) return DI_ERROR;
        out->r_offset = (OPERAND_SIZE)DianaElf_rd32(buf + 0, isLE);
        info = DianaElf_rd32(buf + 4, isLE);
        out->r_sym = (DI_UINT32)DIANA_ELF_R_SYM_32(info);
        out->r_type = (DI_UINT32)DIANA_ELF_R_TYPE_32(info);
        out->r_addend = (DI_INT64)(DI_INT32)DianaElf_rd32(buf + 8, isLE);
    }
    return DI_SUCCESS;
}


/*
 * Walk an entire REL or RELA table located at VA `tableVA` in the
 * already-mapped output stream, applying each entry.
 *
 * isRela  : 1 = RELA (explicit addend), 0 = REL (implicit addend)
 */
static int DianaElf_ProcessRelocTable(DianaReadWriteRandomStream* pOut,
    OPERAND_SIZE                tableVA,
    OPERAND_SIZE                tableSize,
    OPERAND_SIZE                loadBias,
    DI_UINT16                   machine,
    int                         is64,
    int                         isLE,
    int                         isRela)
{
    OPERAND_SIZE entSize = isRela
        ? (is64 ? 24u : 12u)   /* sizeof Elf64_Rela / Elf32_Rela */
        : (is64 ? 16u : 8u);  /* sizeof Elf64_Rel  / Elf32_Rel  */
    OPERAND_SIZE count = 0;
    OPERAND_SIZE i;

    if (entSize == 0 || tableSize == 0)
        return DI_SUCCESS;

    count = tableSize / entSize;

    for (i = 0; i < count; ++i)
    {
        DianaElf_RelaEntry entry;
        OPERAND_SIZE       entVA = tableVA + i * entSize;

        if (isRela)
        {
            DI_CHECK(DianaElf_ReadRela(pOut, entVA, is64, isLE, &entry));
        }
        else
        {
            DI_CHECK(DianaElf_ReadRel(pOut, entVA, is64, isLE, &entry));
        }
        DI_CHECK(DianaElf_ApplyReloc(pOut, loadBias, &entry, machine, is64, isLE));
    }
    return DI_SUCCESS;
}


/*
 * Apply base-address-relative (RELATIVE) and absolute (ABS64/ABS32)
 * fixups to an ELF image that was already mapped into pOutStream by
 * DianaElfFile_MapEx.
 *
 * loadBias  - the value passed as `address` to MapEx.
 *             For ET_EXEC this is typically 0.
 *             For ET_DYN this is the chosen load address.
 * pOutStream - the read/write random-access stream over the mapped image.
 *
 * Symbol-dependent relocations (GLOB_DAT, JUMP_SLOT, �) are NOT resolved
 * here; they are handled by DianaElfFile_QueryImports.
 */
int DianaElfFile_Relocate(/* in */    Diana_ElfFile* pElfFile,
    /* in */    OPERAND_SIZE                loadBias,
    /* inout */ DianaReadWriteRandomStream* pOutStream)
{
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;
    const int            is64 = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_64BIT;
    const int            isLE = pImpl->internalFlags & DIANA_ELF_INTERNAL_FLAG_LE;
    const DI_UINT16      machine = pImpl->elfHeader.e_machine;

    /* ----------------------------------------------------------------
     * For ET_EXEC with loadBias == 0 there is nothing to rebase.
     * ---------------------------------------------------------------- */
    if (loadBias == 0 && pImpl->elfHeader.e_type == DIANA_ET_EXEC)
        return DI_SUCCESS;

    /* ----------------------------------------------------------------
     * Locate the PT_DYNAMIC segment in the already-mapped image and
     * walk its entries to find the relocation tables.
     * ---------------------------------------------------------------- */
    if (pImpl->dynamicSize == 0)
        return DI_SUCCESS;  /* no dynamic segment � nothing to do */

    {
        /* Dynamic segment VA in the mapped image = preferred VA + loadBias */
        OPERAND_SIZE dynVA = (OPERAND_SIZE)pImpl->dynamicAddress + loadBias;
        OPERAND_SIZE dynSize = (OPERAND_SIZE)pImpl->dynamicSize;

        /* Entry size on disk: 16 bytes (32-bit) or 24 bytes (64-bit).        */
        /* We read raw bytes and decode, so we are endian-safe.               */
        OPERAND_SIZE dynEntSize = is64 ? 16u : 8u;  /* d_tag + d_val         */
        OPERAND_SIZE dynCount = dynSize / dynEntSize;

        /* ---- Collect DT_ values we need ---- */
        OPERAND_SIZE relaVA = 0, relaSz = 0;
        OPERAND_SIZE relVA = 0, relSz = 0;
        OPERAND_SIZE jmprelVA = 0, jmprelSz = 0;
        int          pltRelType = DIANA_DT_RELA;   /* default; overridden by DT_PLTREL */

        OPERAND_SIZE i;
        for (i = 0; i < dynCount; ++i)
        {
            OPERAND_SIZE entVA = dynVA + i * dynEntSize;
            OPERAND_SIZE rb = 0;
            DI_INT64     d_tag = 0;
            DI_UINT64    d_val = 0;

            if (is64)
            {
                unsigned char buf[16];
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream, entVA,
                    buf, sizeof(buf), &rb, 0));
                if (rb != sizeof(buf)) return DI_ERROR;
                d_tag = (DI_INT64)DianaElf_rd64(buf + 0, isLE);
                d_val = DianaElf_rd64(buf + 8, isLE);
            }
            else
            {
                unsigned char buf[8];
                DI_CHECK(pOutStream->parent.pRandomRead(pOutStream, entVA,
                    buf, sizeof(buf), &rb, 0));
                if (rb != sizeof(buf)) return DI_ERROR;
                d_tag = (DI_INT64)(DI_INT32)DianaElf_rd32(buf + 0, isLE);
                d_val = (DI_UINT64)DianaElf_rd32(buf + 4, isLE);
            }

            if (d_tag == DIANA_DT_NULL)
                break;

            switch ((int)d_tag)
            {
            case DIANA_DT_RELA:    relaVA = (OPERAND_SIZE)d_val + loadBias; break;
            case DIANA_DT_RELASZ:  relaSz = (OPERAND_SIZE)d_val;            break;
            case DIANA_DT_REL:     relVA = (OPERAND_SIZE)d_val + loadBias; break;
            case DIANA_DT_RELSZ:   relSz = (OPERAND_SIZE)d_val;            break;
            case DIANA_DT_JMPREL:  jmprelVA = (OPERAND_SIZE)d_val + loadBias; break;
            case DIANA_DT_PLTRELSZ:jmprelSz = (OPERAND_SIZE)d_val;            break;
            case DIANA_DT_PLTREL:  pltRelType = (int)d_val;                   break;
            default: break;
            }

            /* Patch pointer-type DT entries with loadBias, mirroring what the
               Linux loader does so that QueryImports/QueryExports can use the
               mapped image without knowing the original load address. */
            if (loadBias != 0)
            {
                int isPtrTag = 0;
                switch ((int)d_tag)
                {
                case DI_ELF_DT_PLTGOT:
                case DI_ELF_DT_HASH:
                case DI_ELF_DT_STRTAB:
                case DI_ELF_DT_SYMTAB:
                case DI_ELF_DT_RELA:
                case DI_ELF_DT_INIT:
                case DI_ELF_DT_FINI:
                case DI_ELF_DT_REL:
                case DI_ELF_DT_JMPREL:
                case DI_ELF_DT_INIT_ARRAY:
                case DI_ELF_DT_FINI_ARRAY:
                    isPtrTag = 1;
                    break;
                default:
                    if ((DI_UINT64)d_tag == DI_ELF_DT_GNU_HASH)
                        isPtrTag = 1;
                    break;
                }
                if (isPtrTag)
                {
                    DI_UINT64 newVal = d_val + (DI_UINT64)loadBias;
                    OPERAND_SIZE valVA = entVA + (OPERAND_SIZE)(is64 ? 8 : 4);
                    if (is64)
                    {
                        DI_CHECK(DianaElf_Write64(pOutStream, valVA, newVal, isLE));
                    }
                    else
                    {
                        DI_CHECK(DianaElf_Write32(pOutStream, valVA, (DI_UINT32)newVal, isLE));
                    }
                }
            }
        }


        /* ----------------------------------------------------------------
         * Process RELA table (.rela.dyn)
         * ---------------------------------------------------------------- */
        if (relaVA != 0 && relaSz != 0)
        {
            DI_CHECK(DianaElf_ProcessRelocTable(pOutStream,
                relaVA, relaSz,
                loadBias, machine,
                is64, isLE,
                1 /* isRela */));
        }

        /* ----------------------------------------------------------------
         * Process REL table (.rel.dyn)
         * ---------------------------------------------------------------- */
        if (relVA != 0 && relSz != 0)
        {
            DI_CHECK(DianaElf_ProcessRelocTable(pOutStream,
                relVA, relSz,
                loadBias, machine,
                is64, isLE,
                0 /* isRel */));
        }

        /* ----------------------------------------------------------------
         * Process PLT relocations (.rela.plt / .rel.plt)
         * Only RELATIVE-class entries are handled here; JUMP_SLOTs are
         * skipped inside DianaElf_ApplyReloc and resolved by QueryImports.
         * ---------------------------------------------------------------- */
        if (jmprelVA != 0 && jmprelSz != 0)
        {
            int isRelaForPlt = (pltRelType == DIANA_DT_RELA) ? 1 : 0;
            DI_CHECK(DianaElf_ProcessRelocTable(pOutStream,
                jmprelVA, jmprelSz,
                loadBias, machine,
                is64, isLE,
                isRelaForPlt));
        }
    }
    return DI_SUCCESS;
}

int DianaElfFile_MapEx(/* in */  Diana_ElfFile* pElfFile,
    /* in */  DianaMovableReadStream* pStream,
    /* in */  OPERAND_SIZE                 address,
    /* inout*/DianaReadWriteRandomStream* pOutStream,
    /* in */  void* pPage,
    /* in */  int                          pageSize,
    /* in */  int                          flags)
{
    int i = 0;
    Diana_ElfFile_impl* pImpl = pElfFile->pImpl;

    // ----------------------------------------------------------------
    // Basic sanity checks (mirror the PE version)
    // ----------------------------------------------------------------
    if ((pElfFile->flags & DIANA_ELF_FILE_FLAGS_FILE_MODE) == 0)
    {
        return DI_INVALID_INPUT;
    }
    if (pImpl->sizeOfModule == 0)
    {
        return DI_ERROR;
    }
    if (pageSize < (int)sizeof(OPERAND_SIZE))
    {
        return DI_ERROR;
    }

    // ----------------------------------------------------------------
    // Map the ELF header + program-header table.
    //
    // In ELF there is no dedicated "sizeOfHeaders" field like in PE.
    // The conventional approach is to use the first PT_LOAD segment's
    // p_offset == 0 coverage, but we stay safe and just copy
    // e_ehsize + all program header entries (e_phnum * e_phentsize).
    // Any PT_LOAD with p_offset == 0 will overwrite/confirm this below.
    // ----------------------------------------------------------------
    {
        OPERAND_SIZE headerRegionSize =
            (OPERAND_SIZE)pImpl->elfHeader.e_ehsize +
            (OPERAND_SIZE)pImpl->elfHeader.e_phnum *
            (OPERAND_SIZE)pImpl->elfHeader.e_phentsize;

        if (headerRegionSize == 0 || headerRegionSize > pImpl->sizeOfModule)
        {
            return DI_ERROR;
        }

        DI_CHECK(pStream->pMoveTo(pStream, 0));
        DI_CHECK(DianaStreams_CopyData(&pStream->parent,
            pOutStream,
            address,          /* dest VA */
            headerRegionSize,
            pPage,
            pageSize));
    }

    // ----------------------------------------------------------------
    // Map each PT_LOAD segment  (analogous to PE sections)
    //
    // ELF field mapping vs PE:
    //   p_offset   <-->  PointerToRawData
    //   p_filesz   <-->  SizeOfRawData
    //   p_memsz    <-->  VirtualSize
    //   p_vaddr    <-->  VirtualAddress
    // ----------------------------------------------------------------
    for (i = 0; i < pImpl->capturedSegmentCount; ++i)
    {
        DIANA_ELF_PROGRAM_HEADER* pPhdr = pImpl->pCapturedSegments + i;

        OPERAND_SIZE segmentVA = 0;
        OPERAND_SIZE segmentVAEnd = 0;

        DI_UINT64 fileSize = pPhdr->p_filesz;   /* bytes present in file */
        DI_UINT64 memSize = pPhdr->p_memsz;    /* bytes needed in memory */

        // Only loadable segments are mapped.
        if (pPhdr->p_type != DIANA_PT_LOAD)
        {
            continue;
        }

        // Bounds checks
        if (pPhdr->p_offset > pImpl->sizeOfModule ||
            fileSize > pImpl->sizeOfModule)
        {
            return DI_ERROR;
        }

        // A segment with memsz == 0 is degenerate; treat like PE virtualSize==0
        if (memSize == 0)
        {
            memSize = fileSize;
        }

        // If no file data, nothing to read (BSS-style segment)
        if (pPhdr->p_offset == 0 && fileSize == 0)
        {
            fileSize = 0;
        }
        else if (fileSize > memSize)
        {
            // File data cannot exceed virtual extent (clamp, same as PE logic)
            fileSize = memSize;
        }

        // Destination virtual address = load bias + segment's preferred VA
        segmentVA = (OPERAND_SIZE)pPhdr->p_vaddr;
        DI_CHECK(Diana_SafeAdd(&segmentVA, address));

        segmentVAEnd = segmentVA;
        DI_CHECK(Diana_SafeAdd(&segmentVAEnd, (OPERAND_SIZE)fileSize));

        // Copy file bytes into the output stream
        if (fileSize != 0)
        {
            DI_CHECK(pStream->pMoveTo(pStream,
                (OPERAND_SIZE)pPhdr->p_offset));

            DI_CHECK(DianaStreams_CopyData(&pStream->parent,
                pOutStream,
                segmentVA,
                (OPERAND_SIZE)fileSize,
                pPage,
                pageSize));
        }

        // Zero the .bss tail  (memsz > filesz)
        if (fileSize < memSize)
        {
            DI_CHECK(DianaStreams_MemsetData(pOutStream,
                segmentVAEnd,
                (OPERAND_SIZE)(memSize - fileSize),
                0,
                pPage,
                pageSize));
        }
    }

    if (flags & DIANA_ELF_MAP_DO_NOT_RELOCATE)
    {
        return DI_SUCCESS;
    }

    // ----------------------------------------------------------------
    // Relocations (placeholder � implement similarly to DianaPeFile_Relocate)
    // ----------------------------------------------------------------
    return DianaElfFile_Relocate(pElfFile,
        address,
        pOutStream);
}
