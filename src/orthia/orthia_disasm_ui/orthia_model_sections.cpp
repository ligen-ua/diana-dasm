#include "orthia_model_sections.h"
#include "orthia_utils.h"
extern "C"
{
#include "diana_pe_defs.h"
#include "diana_elfs_defs.h"
}

namespace orthia
{

namespace
{

typedef struct {
    DI_UINT32 sh_name;
    DI_UINT32 sh_type;
    DI_UINT32 sh_flags;
    DI_UINT32 sh_addr;
    DI_UINT32 sh_offset;
    DI_UINT32 sh_size;
    DI_UINT32 sh_link;
    DI_UINT32 sh_info;
    DI_UINT32 sh_addralign;
    DI_UINT32 sh_entsize;
} Elf32_Shdr;

static bool ReadMemory(IMemoryReader* reader, Address_type address, void* buffer, size_t size)
{
    Address_type bytesRead = 0;
    reader->Read(address, (Address_type)size, buffer, &bytesRead,
                 ORTHIA_MR_FLAG_READ_ABSOLUTE, 0, reg_none);
    return bytesRead == (Address_type)size;
}

static oui::String MakeHex(unsigned long long v)
{
    return oui::String(ToWideStringAsHex(v));
}

static oui::String MakeLit(const char* s)
{
    return oui::String(Utf8ToPlatformString(s));
}

static bool TryParsePeSections(IMemoryReader* reader, Address_type moduleBase, std::vector<SectionInfo>& out)
{
    DIANA_IMAGE_DOS_HEADER dosHeader = {};
    if (!ReadMemory(reader, moduleBase, &dosHeader, sizeof(dosHeader)))
        return false;
    if (dosHeader.e_magic[0] != 'M' || dosHeader.e_magic[1] != 'Z')
        return false;

    DIANA_IMAGE_NT_HEADERS ntHeaders = {};
    Address_type ntOffset = moduleBase + (DI_UINT32)dosHeader.e_lfanew;
    if (!ReadMemory(reader, ntOffset, &ntHeaders, sizeof(ntHeaders)))
        return false;
    if (ntHeaders.Signature[0] != 'P' || ntHeaders.Signature[1] != 'E')
        return false;

    DI_UINT16 numSections = ntHeaders.FileHeader.NumberOfSections;
    DI_UINT16 optHdrSize  = ntHeaders.FileHeader.SizeOfOptionalHeader;
    if (numSections == 0 || numSections > 96)
        return false;

    Address_type sectionOffset = ntOffset + sizeof(ntHeaders) + optHdrSize;
    out.reserve(numSections);

    for (DI_UINT16 i = 0; i < numSections; ++i)
    {
        DIANA_IMAGE_SECTION_HEADER sh = {};
        if (!ReadMemory(reader, sectionOffset + i * sizeof(sh), &sh, sizeof(sh)))
            break;

        SectionInfo info;
        char nameStr[9] = {};
        memcpy(nameStr, sh.Name, 8);
        info.name = oui::String(Utf8ToPlatformString(nameStr));
        info.virtualAddress = moduleBase + sh.VirtualAddress;
        info.size = sh.Misc.VirtualSize;

        std::string flagsStr;
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_READ)    ? 'R' : '-';
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_WRITE)   ? 'W' : '-';
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_EXECUTE) ? 'X' : '-';
        info.flagsShort = oui::String(Utf8ToPlatformString(flagsStr));

        info.attributes.push_back({ MakeLit("VirtualAddress"),       MakeHex(moduleBase + sh.VirtualAddress) });
        info.attributes.push_back({ MakeLit("VirtualSize"),          MakeHex(sh.Misc.VirtualSize) });
        info.attributes.push_back({ MakeLit("SizeOfRawData"),        MakeHex(sh.SizeOfRawData) });
        info.attributes.push_back({ MakeLit("PointerToRawData"),     MakeHex(sh.PointerToRawData) });
        info.attributes.push_back({ MakeLit("PointerToRelocations"), MakeHex(sh.PointerToRelocations) });
        info.attributes.push_back({ MakeLit("NumberOfRelocations"),  MakeHex(sh.NumberOfRelocations) });
        info.attributes.push_back({ MakeLit("Characteristics"),      MakeHex(sh.Characteristics) });

        std::string decoded;
        if (sh.Characteristics & DIANA_IMAGE_SCN_CNT_CODE)               decoded += "CODE ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_CNT_INITIALIZED_DATA)   decoded += "INITIALIZED_DATA ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_CNT_UNINITIALIZED_DATA) decoded += "UNINITIALIZED_DATA ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_MEM_DISCARDABLE)        decoded += "DISCARDABLE ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_MEM_EXECUTE)            decoded += "EXECUTE ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_MEM_READ)               decoded += "READ ";
        if (sh.Characteristics & DIANA_IMAGE_SCN_MEM_WRITE)              decoded += "WRITE ";
        if (!decoded.empty())
            info.attributes.push_back({ MakeLit(""), oui::String(Utf8ToPlatformString(decoded)) });

        out.push_back(std::move(info));
    }
    return true;
}

static const char* ElfShtName(DI_UINT32 type)
{
    switch (type)
    {
    case DIANA_SHT_NULL:          return "SHT_NULL";
    case DIANA_SHT_PROGBITS:      return "SHT_PROGBITS";
    case DIANA_SHT_SYMTAB:        return "SHT_SYMTAB";
    case DIANA_SHT_STRTAB:        return "SHT_STRTAB";
    case DIANA_SHT_RELA:          return "SHT_RELA";
    case DIANA_SHT_HASH:          return "SHT_HASH";
    case DIANA_SHT_DYNAMIC:       return "SHT_DYNAMIC";
    case DIANA_SHT_NOTE:          return "SHT_NOTE";
    case DIANA_SHT_NOBITS:        return "SHT_NOBITS";
    case DIANA_SHT_REL:           return "SHT_REL";
    case DIANA_SHT_DYNSYM:        return "SHT_DYNSYM";
    case DIANA_SHT_INIT_ARRAY:    return "SHT_INIT_ARRAY";
    case DIANA_SHT_FINI_ARRAY:    return "SHT_FINI_ARRAY";
    default:                      return nullptr;
    }
}

template<class ElfHdr, class ShHdr>
static void ParseElfSections(IMemoryReader* reader, Address_type base,
                              const ElfHdr& hdr, std::vector<SectionInfo>& out)
{
    if (hdr.e_shnum == 0 || hdr.e_shnum > 256 || hdr.e_shentsize < sizeof(ShHdr))
        return;

    // Read section name string table (shstrtab)
    std::vector<char> shstrtab;
    if (hdr.e_shstrndx != 0 && hdr.e_shstrndx < hdr.e_shnum)
    {
        ShHdr strShdr = {};
        Address_type strShdrAddr = base + (Address_type)hdr.e_shoff + (Address_type)hdr.e_shstrndx * sizeof(ShHdr);
        if (ReadMemory(reader, strShdrAddr, &strShdr, sizeof(strShdr)) && strShdr.sh_size > 0 && strShdr.sh_size < 0x100000)
        {
            shstrtab.resize((size_t)strShdr.sh_size);
            if (!ReadMemory(reader, base + (Address_type)strShdr.sh_offset, shstrtab.data(), shstrtab.size()))
                shstrtab.clear();
        }
    }

    out.reserve(hdr.e_shnum);
    for (DI_UINT16 i = 0; i < hdr.e_shnum; ++i)
    {
        ShHdr sh = {};
        Address_type shAddr = base + (Address_type)hdr.e_shoff + (Address_type)i * sizeof(ShHdr);
        if (!ReadMemory(reader, shAddr, &sh, sizeof(sh)))
            break;

        SectionInfo info;

        // section name from string table
        if (sh.sh_name < shstrtab.size())
        {
            const char* namePtr = shstrtab.data() + sh.sh_name;
            size_t maxLen = shstrtab.size() - sh.sh_name;
            size_t len = strnlen(namePtr, maxLen);
            info.name = oui::String(Utf8ToPlatformString(std::string(namePtr, len)));
        }

        info.virtualAddress = (Address_type)sh.sh_addr;
        info.size = (Address_type)sh.sh_size;

        std::string flagsStr;
        flagsStr += (sh.sh_flags & DIANA_SHF_ALLOC)     ? 'A' : '-';
        flagsStr += (sh.sh_flags & DIANA_SHF_WRITE)     ? 'W' : '-';
        flagsStr += (sh.sh_flags & DIANA_SHF_EXECINSTR) ? 'X' : '-';
        info.flagsShort = oui::String(Utf8ToPlatformString(flagsStr));

        const char* typeName = ElfShtName(sh.sh_type);
        info.attributes.push_back({ MakeLit("sh_type"),
            typeName ? MakeLit(typeName) : MakeHex(sh.sh_type) });
        info.attributes.push_back({ MakeLit("sh_flags"),  MakeHex((unsigned long long)sh.sh_flags) });

        std::string decoded;
        if (sh.sh_flags & DIANA_SHF_WRITE)     decoded += "WRITE ";
        if (sh.sh_flags & DIANA_SHF_ALLOC)     decoded += "ALLOC ";
        if (sh.sh_flags & DIANA_SHF_EXECINSTR) decoded += "EXECINSTR ";
        if (sh.sh_flags & DIANA_SHF_MERGE)     decoded += "MERGE ";
        if (sh.sh_flags & DIANA_SHF_STRINGS)   decoded += "STRINGS ";
        if (sh.sh_flags & DIANA_SHF_TLS)       decoded += "TLS ";
        if (!decoded.empty())
            info.attributes.push_back({ MakeLit(""), oui::String(Utf8ToPlatformString(decoded)) });

        info.attributes.push_back({ MakeLit("sh_addr"),     MakeHex((unsigned long long)sh.sh_addr) });
        info.attributes.push_back({ MakeLit("sh_offset"),   MakeHex((unsigned long long)sh.sh_offset) });
        info.attributes.push_back({ MakeLit("sh_size"),     MakeHex((unsigned long long)sh.sh_size) });
        info.attributes.push_back({ MakeLit("sh_addralign"),MakeHex((unsigned long long)sh.sh_addralign) });

        out.push_back(std::move(info));
    }
}

static bool TryParseElfSections(IMemoryReader* reader, Address_type moduleBase, std::vector<SectionInfo>& out)
{
    // Read ELF ident (16 bytes)
    DI_UINT8 ident[DIANA_EI_NIDENT] = {};
    if (!ReadMemory(reader, moduleBase, ident, sizeof(ident)))
        return false;
    if (ident[0] != DIANA_ELFMAG0 || ident[1] != DIANA_ELFMAG1 ||
        ident[2] != DIANA_ELFMAG2 || ident[3] != DIANA_ELFMAG3)
        return false;

    if (ident[DIANA_EI_CLASS] == DIANA_ELFCLASS64)
    {
        DIANA_ELF_HEADER hdr = {};
        if (!ReadMemory(reader, moduleBase, &hdr, sizeof(hdr)))
            return false;
        ParseElfSections<DIANA_ELF_HEADER, DIANA_ELF_SECTION_HEADER>(reader, moduleBase, hdr, out);
    }
    else if (ident[DIANA_EI_CLASS] == DIANA_ELFCLASS32)
    {
        Elf32_Ehdr hdr = {};
        if (!ReadMemory(reader, moduleBase, &hdr, sizeof(hdr)))
            return false;
        ParseElfSections<Elf32_Ehdr, Elf32_Shdr>(reader, moduleBase, hdr, out);
    }
    else
    {
        return false;
    }
    return true;
}

} // anonymous namespace

void QuerySectionsImpl(IMemoryReader* reader, Address_type moduleBase, std::vector<SectionInfo>& sections_out)
{
    sections_out.clear();
    if (!reader || !moduleBase)
        return;
    if (TryParsePeSections(reader, moduleBase, sections_out))
        return;
    TryParseElfSections(reader, moduleBase, sections_out);
}

} // namespace orthia
