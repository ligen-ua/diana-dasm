#include "orthia_sections.h"
#include <filesystem>
#include <fstream>
extern "C"
{
#include "diana_pe_defs.h"
#include "diana_elfs_defs.h"
}

namespace orthia
{

namespace
{

bool ReadMemory(IMemoryReader* reader, Address_type address, void* buffer, size_t size)
{
    Address_type bytesRead = 0;
    try
    {
        reader->Read(address, (Address_type)size, buffer, &bytesRead,
                     ORTHIA_MR_FLAG_READ_ABSOLUTE, 0, reg_none);
    }
    catch (const std::exception&)
    {
        return false;
    }
    return bytesRead == (Address_type)size;
}

PlatformString_type MakeHex(unsigned long long v)
{
    return ToWideStringAsHex(v);
}

PlatformString_type MakeLit(const char* s)
{
    return Utf8ToPlatformString(s);
}

bool TryParsePeSections(IMemoryReader* reader, Address_type moduleBase, std::vector<ImageSection>& out)
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

        ImageSection info;
        char nameStr[9] = {};
        memcpy(nameStr, sh.Name, 8);
        info.name = Utf8ToPlatformString(nameStr);
        info.address = moduleBase + sh.VirtualAddress;
        info.size = sh.Misc.VirtualSize;

        std::string flagsStr;
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_READ)    ? 'R' : '-';
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_WRITE)   ? 'W' : '-';
        flagsStr += (sh.Characteristics & DIANA_IMAGE_SCN_MEM_EXECUTE) ? 'X' : '-';
        info.flagsShort = Utf8ToPlatformString(flagsStr);

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
            info.attributes.push_back({ MakeLit(""), Utf8ToPlatformString(decoded) });

        out.push_back(std::move(info));
    }
    return true;
}

// ELF: fields are decoded from raw bytes, so both classes and both byte orders share one parser

const DI_UINT32 c_pt_gnuEhFrame  = 0x6474e550;
const DI_UINT32 c_pt_gnuStack    = 0x6474e551;
const DI_UINT32 c_pt_gnuRelro    = 0x6474e552;
const DI_UINT32 c_pt_gnuProperty = 0x6474e553;

const DI_UINT32 c_sht_gnuHash    = 0x6ffffff6;
const DI_UINT32 c_sht_gnuVerdef  = 0x6ffffffd;
const DI_UINT32 c_sht_gnuVerneed = 0x6ffffffe;
const DI_UINT32 c_sht_gnuVersym  = 0x6fffffff;

const DI_UINT16 c_shn_xindex = 0xffff;
const DI_UINT16 c_shn_loreserve = 0xff00;

const Address_type c_pageMask = 0xfff;
const size_t c_maxShstrtabSize = 0x1000000;

struct ElfLayout
{
    bool is64 = false;
    bool le = true;
    DI_UINT16 type = 0;
    unsigned long long phoff = 0;
    unsigned long long shoff = 0;
    DI_UINT16 ehsize = 0;
    DI_UINT16 phentsize = 0;
    DI_UINT16 phnum = 0;
    DI_UINT16 shentsize = 0;
    DI_UINT16 shnum = 0;
    DI_UINT16 shstrndx = 0;

    size_t HeaderSize() const { return is64 ? 64 : 52; }
    size_t PhdrSize() const { return is64 ? 56 : 32; }
    size_t ShdrSize() const { return is64 ? 64 : 40; }

    unsigned long long Get(const char* p, size_t size) const
    {
        unsigned long long value = 0;
        for (size_t i = 0; i < size; ++i)
        {
            unsigned long long byte = (unsigned char)p[le ? i : size - 1 - i];
            value |= byte << (8 * i);
        }
        return value;
    }
    // a field that is 4 bytes in ELF32 and 8 bytes in ELF64
    unsigned long long GetWord(const char* p) const { return Get(p, is64 ? 8 : 4); }
};

struct ElfSegment
{
    DI_UINT32 type = 0;
    DI_UINT32 flags = 0;
    unsigned long long offset = 0, vaddr = 0, filesz = 0, memsz = 0, align = 0;
};

struct ElfSection
{
    DI_UINT32 name = 0;
    DI_UINT32 type = 0;
    unsigned long long flags = 0, addr = 0, offset = 0, size = 0;
    DI_UINT32 link = 0, info = 0;
    unsigned long long addralign = 0, entsize = 0;
};

// header: at least 64 bytes
bool ParseElfHeader(const char* header, ElfLayout& layout)
{
    if (header[0] != DIANA_ELFMAG0 || header[1] != DIANA_ELFMAG1 ||
        header[2] != DIANA_ELFMAG2 || header[3] != DIANA_ELFMAG3)
        return false;
    if (header[DIANA_EI_CLASS] == DIANA_ELFCLASS64)
        layout.is64 = true;
    else if (header[DIANA_EI_CLASS] == DIANA_ELFCLASS32)
        layout.is64 = false;
    else
        return false;
    if (header[DIANA_EI_DATA] == DIANA_ELFDATA2LSB)
        layout.le = true;
    else if (header[DIANA_EI_DATA] == DIANA_ELFDATA2MSB)
        layout.le = false;
    else
        return false;

    layout.type = (DI_UINT16)layout.Get(header + 16, 2);
    const char* p = header + 24 + (layout.is64 ? 8 : 4);   // after e_entry
    const size_t word = layout.is64 ? 8 : 4;
    layout.phoff     = layout.GetWord(p);
    layout.shoff     = layout.GetWord(p + word);
    p += 2 * word + 4;                                      // e_flags
    layout.ehsize    = (DI_UINT16)layout.Get(p, 2);
    layout.phentsize = (DI_UINT16)layout.Get(p + 2, 2);
    layout.phnum     = (DI_UINT16)layout.Get(p + 4, 2);
    layout.shentsize = (DI_UINT16)layout.Get(p + 6, 2);
    layout.shnum     = (DI_UINT16)layout.Get(p + 8, 2);
    layout.shstrndx  = (DI_UINT16)layout.Get(p + 10, 2);
    return true;
}

ElfSegment ParseElfSegment(const ElfLayout& layout, const char* p)
{
    ElfSegment seg;
    seg.type = (DI_UINT32)layout.Get(p, 4);
    if (layout.is64)
    {
        seg.flags  = (DI_UINT32)layout.Get(p + 4, 4);
        seg.offset = layout.Get(p + 8, 8);
        seg.vaddr  = layout.Get(p + 16, 8);
        seg.filesz = layout.Get(p + 32, 8);
        seg.memsz  = layout.Get(p + 40, 8);
        seg.align  = layout.Get(p + 48, 8);
    }
    else
    {
        seg.offset = layout.Get(p + 4, 4);
        seg.vaddr  = layout.Get(p + 8, 4);
        seg.filesz = layout.Get(p + 16, 4);
        seg.memsz  = layout.Get(p + 20, 4);
        seg.flags  = (DI_UINT32)layout.Get(p + 24, 4);
        seg.align  = layout.Get(p + 28, 4);
    }
    return seg;
}

ElfSection ParseElfSection(const ElfLayout& layout, const char* p)
{
    ElfSection sec;
    sec.name = (DI_UINT32)layout.Get(p, 4);
    sec.type = (DI_UINT32)layout.Get(p + 4, 4);
    const size_t word = layout.is64 ? 8 : 4;
    p += 8;
    sec.flags     = layout.GetWord(p);
    sec.addr      = layout.GetWord(p + word);
    sec.offset    = layout.GetWord(p + 2 * word);
    sec.size      = layout.GetWord(p + 3 * word);
    p += 4 * word;
    sec.link      = (DI_UINT32)layout.Get(p, 4);
    sec.info      = (DI_UINT32)layout.Get(p + 4, 4);
    sec.addralign = layout.GetWord(p + 8);
    sec.entsize   = layout.GetWord(p + 8 + word);
    return sec;
}

const char* ElfPtName(DI_UINT32 type)
{
    switch (type)
    {
    case DIANA_PT_LOAD:     return "LOAD";
    case DIANA_PT_DYNAMIC:  return "DYNAMIC";
    case DIANA_PT_INTERP:   return "INTERP";
    case DIANA_PT_NOTE:     return "NOTE";
    case DIANA_PT_SHLIB:    return "SHLIB";
    case DIANA_PT_PHDR:     return "PHDR";
    case DIANA_PT_TLS:      return "TLS";
    case c_pt_gnuEhFrame:   return "GNU_EH_FRAME";
    case c_pt_gnuStack:     return "GNU_STACK";
    case c_pt_gnuRelro:     return "GNU_RELRO";
    case c_pt_gnuProperty:  return "GNU_PROPERTY";
    default:                return nullptr;
    }
}

const char* ElfShtName(DI_UINT32 type)
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
    case DIANA_SHT_PREINIT_ARRAY: return "SHT_PREINIT_ARRAY";
    case DIANA_SHT_GROUP:         return "SHT_GROUP";
    case DIANA_SHT_SYMTAB_SHNDX:  return "SHT_SYMTAB_SHNDX";
    case DIANA_SHT_RELR:          return "SHT_RELR";
    case c_sht_gnuHash:           return "SHT_GNU_HASH";
    case c_sht_gnuVerdef:         return "SHT_GNU_verdef";
    case c_sht_gnuVerneed:        return "SHT_GNU_verneed";
    case c_sht_gnuVersym:         return "SHT_GNU_versym";
    default:                      return nullptr;
    }
}

// the ELF and program headers as mapped at the module base
struct MappedElf
{
    ElfLayout layout;
    std::vector<char> header;
    std::vector<char> phdrs;
    std::vector<ElfSegment> segments;
    Address_type loadBias = 0;
};

bool ReadMappedElf(IMemoryReader* reader, Address_type moduleBase, MappedElf& elf)
{
    elf.header.resize(64);
    if (!ReadMemory(reader, moduleBase, elf.header.data(), elf.header.size()))
        return false;
    if (!ParseElfHeader(elf.header.data(), elf.layout))
        return false;
    elf.header.resize(elf.layout.HeaderSize());

    const auto& layout = elf.layout;
    if (layout.phnum && layout.phentsize >= layout.PhdrSize())
    {
        elf.phdrs.resize((size_t)layout.phnum * layout.phentsize);
        if (!ReadMemory(reader, moduleBase + layout.phoff, elf.phdrs.data(), elf.phdrs.size()))
            elf.phdrs.clear();
    }
    bool haveLoad = false;
    Address_type minLoadVaddr = 0;
    for (size_t offset = 0; offset < elf.phdrs.size(); offset += layout.phentsize)
    {
        auto seg = ParseElfSegment(layout, elf.phdrs.data() + offset);
        if (seg.type == DIANA_PT_LOAD && (!haveLoad || seg.vaddr < minLoadVaddr))
        {
            minLoadVaddr = seg.vaddr;
            haveLoad = true;
        }
        elf.segments.push_back(seg);
    }
    // the module base is where the lowest PT_LOAD page is mapped
    elf.loadBias = moduleBase - (minLoadVaddr & ~c_pageMask);
    return true;
}

void ListElfSegments(const MappedElf& elf, std::vector<ImageSection>& out)
{
    for (const auto& seg : elf.segments)
    {
        if (seg.type == DIANA_PT_NULL || !seg.memsz)
            continue;
        ImageSection info;
        const char* typeName = ElfPtName(seg.type);
        info.name = typeName ? MakeLit(typeName) : MakeHex(seg.type);
        info.address = elf.loadBias + seg.vaddr;
        info.size = seg.memsz;

        std::string flagsStr;
        flagsStr += (seg.flags & DIANA_PF_R) ? 'R' : '-';
        flagsStr += (seg.flags & DIANA_PF_W) ? 'W' : '-';
        flagsStr += (seg.flags & DIANA_PF_X) ? 'X' : '-';
        info.flagsShort = Utf8ToPlatformString(flagsStr);

        info.attributes.push_back({ MakeLit("p_type"),   typeName ? MakeLit(typeName) : MakeHex(seg.type) });
        info.attributes.push_back({ MakeLit("p_flags"),  MakeHex(seg.flags) });
        info.attributes.push_back({ MakeLit("p_offset"), MakeHex(seg.offset) });
        info.attributes.push_back({ MakeLit("p_vaddr"),  MakeHex(seg.vaddr) });
        info.attributes.push_back({ MakeLit("p_filesz"), MakeHex(seg.filesz) });
        info.attributes.push_back({ MakeLit("p_memsz"),  MakeHex(seg.memsz) });
        info.attributes.push_back({ MakeLit("p_align"),  MakeHex(seg.align) });
        out.push_back(std::move(info));
    }
}

class CFileRangeReader
{
    std::ifstream m_file;
    unsigned long long m_size = 0;
public:
    bool Open(const PlatformString_type& name)
    {
        std::error_code error;
        std::filesystem::path path(name);
        m_size = std::filesystem::file_size(path, error);
        if (error)
            return false;
        m_file.open(path, std::ios::binary);
        return m_file.is_open();
    }
    unsigned long long GetSize() const { return m_size; }
    bool Read(unsigned long long offset, std::vector<char>& data)
    {
        if (offset > m_size || data.size() > m_size - offset)
            return false;
        m_file.clear();
        m_file.seekg((std::streamoff)offset);
        m_file.read(data.data(), (std::streamsize)data.size());
        return (size_t)m_file.gcount() == data.size();
    }
};

void AddElfSection(const ElfLayout& layout, const ElfSection& sec, const std::vector<char>& shstrtab,
    Address_type loadBias, std::vector<ImageSection>& out)
{
    ImageSection info;
    if (sec.name < shstrtab.size())
    {
        const char* namePtr = shstrtab.data() + sec.name;
        size_t len = strnlen(namePtr, shstrtab.size() - sec.name);
        info.name = Utf8ToPlatformString(std::string(namePtr, len));
    }
    // only SHF_ALLOC sections are loaded; the others have no address in memory
    if (sec.flags & DIANA_SHF_ALLOC)
        info.address = loadBias + sec.addr;
    info.size = sec.size;

    std::string flagsStr;
    flagsStr += (sec.flags & DIANA_SHF_ALLOC)     ? 'R' : '-';
    flagsStr += (sec.flags & DIANA_SHF_WRITE)     ? 'W' : '-';
    flagsStr += (sec.flags & DIANA_SHF_EXECINSTR) ? 'X' : '-';
    info.flagsShort = Utf8ToPlatformString(flagsStr);

    const char* typeName = ElfShtName(sec.type);
    info.attributes.push_back({ MakeLit("sh_type"), typeName ? MakeLit(typeName) : MakeHex(sec.type) });
    info.attributes.push_back({ MakeLit("sh_flags"), MakeHex(sec.flags) });

    std::string decoded;
    if (sec.flags & DIANA_SHF_WRITE)     decoded += "WRITE ";
    if (sec.flags & DIANA_SHF_ALLOC)     decoded += "ALLOC ";
    if (sec.flags & DIANA_SHF_EXECINSTR) decoded += "EXECINSTR ";
    if (sec.flags & DIANA_SHF_MERGE)     decoded += "MERGE ";
    if (sec.flags & DIANA_SHF_STRINGS)   decoded += "STRINGS ";
    if (sec.flags & DIANA_SHF_INFO_LINK) decoded += "INFO_LINK ";
    if (sec.flags & DIANA_SHF_GROUP)     decoded += "GROUP ";
    if (sec.flags & DIANA_SHF_TLS)       decoded += "TLS ";
    if (!decoded.empty())
        info.attributes.push_back({ MakeLit(""), Utf8ToPlatformString(decoded) });

    info.attributes.push_back({ MakeLit("sh_addr"),      MakeHex(sec.addr) });
    info.attributes.push_back({ MakeLit("sh_offset"),    MakeHex(sec.offset) });
    info.attributes.push_back({ MakeLit("sh_size"),      MakeHex(sec.size) });
    info.attributes.push_back({ MakeLit("sh_link"),      MakeHex(sec.link) });
    info.attributes.push_back({ MakeLit("sh_info"),      MakeHex(sec.info) });
    info.attributes.push_back({ MakeLit("sh_addralign"), MakeHex(sec.addralign) });
    info.attributes.push_back({ MakeLit("sh_entsize"),   MakeHex(sec.entsize) });
    out.push_back(std::move(info));
}

// Returns an empty string on success, otherwise why the file's section headers can't be used.
std::string ReadElfFileSections(const MappedElf& mapped, const PlatformString_type& imageFile,
    std::vector<ImageSection>& out)
{
    if (imageFile.empty())
        return "section headers are not loaded in memory and the module has no file";

    CFileRangeReader file;
    if (!file.Open(imageFile))
        return "can't open " + PlatformStringToUtf8(imageFile);

    // the file must be the image that is mapped: same ELF header, same program headers
    std::vector<char> header(mapped.header.size());
    std::vector<char> phdrs(mapped.phdrs.size());
    if (!file.Read(0, header) || header != mapped.header ||
        !file.Read(mapped.layout.phoff, phdrs) || phdrs != mapped.phdrs)
        return PlatformStringToUtf8(imageFile) + " is not the loaded image";

    const auto& layout = mapped.layout;
    if (!layout.shoff)
        return "the file has no section headers";
    if (layout.shentsize < layout.ShdrSize())
        return "invalid section headers in the file";

    // section 0 is all zeroes, except for the extended section count and string table index
    std::vector<char> raw(layout.shentsize);
    if (!file.Read(layout.shoff, raw))
        return "invalid section headers in the file";
    auto first = ParseElfSection(layout, raw.data());
    if (first.type != DIANA_SHT_NULL || first.name || first.flags || first.addr || first.offset)
        return "invalid section headers in the file";

    unsigned long long count = layout.shnum ? layout.shnum : first.size;
    unsigned long long strndx = layout.shstrndx;
    if (layout.shstrndx == c_shn_xindex)
        strndx = first.link;
    else if (layout.shstrndx >= c_shn_loreserve)
        strndx = 0;
    if (!count || count > (file.GetSize() - layout.shoff) / layout.shentsize)
        return "invalid section headers in the file";

    raw.resize((size_t)(count * layout.shentsize));
    if (!file.Read(layout.shoff, raw))
        return "invalid section headers in the file";
    std::vector<ElfSection> sections;
    sections.reserve((size_t)count);
    for (size_t offset = 0; offset < raw.size(); offset += layout.shentsize)
    {
        sections.push_back(ParseElfSection(layout, raw.data() + offset));
    }

    std::vector<char> shstrtab;
    if (strndx)
    {
        if (strndx >= count)
            return "invalid section headers in the file";
        const auto& strSection = sections[(size_t)strndx];
        if (strSection.type != DIANA_SHT_STRTAB || strSection.size > c_maxShstrtabSize)
            return "invalid section name table in the file";
        shstrtab.resize((size_t)strSection.size);
        if (!file.Read(strSection.offset, shstrtab))
            return "invalid section name table in the file";
    }

    for (size_t i = 1; i < sections.size(); ++i)
    {
        AddElfSection(layout, sections[i], shstrtab, mapped.loadBias, out);
    }
    return std::string();
}

bool TryParseElfSections(IMemoryReader* reader, Address_type moduleBase, const PlatformString_type& imageFile,
    ImageSections& result)
{
    MappedElf mapped;
    if (!ReadMappedElf(reader, moduleBase, mapped))
        return false;

    auto reason = ReadElfFileSections(mapped, imageFile, result.sections);
    if (reason.empty())
    {
        result.source = imageFile;
        return true;
    }
    result.sections.clear();
    result.segments = true;
    result.reason = Utf8ToPlatformString(reason);
    ListElfSegments(mapped, result.sections);
    return true;
}

} // anonymous namespace

void QueryImageSections(IMemoryReader* reader,
    Address_type moduleBase,
    const PlatformString_type& imageFile,
    ImageSections& result)
{
    result = ImageSections();
    if (!reader || !moduleBase)
        return;
    if (TryParsePeSections(reader, moduleBase, result.sections))
        return;
    TryParseElfSections(reader, moduleBase, imageFile, result);
}

} // namespace orthia
