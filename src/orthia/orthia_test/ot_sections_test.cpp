#include "test_common.h"
#include "orthia_sections.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_elf.h"
#include <algorithm>

std::vector<char> LoadElfTestFile(const orthia::PlatformString_type& name);
orthia::PlatformString_type ElfTestFilePath(const orthia::PlatformString_type& name);
bool ElfTestFilesPresent(const char* testName, std::initializer_list<orthia::PlatformString_type> names);

// opens test files directly; in the app the opener goes through the filesystem interface
class CVectorFileReader : public orthia::IFileRangeReader
{
    std::vector<char> m_data;
public:
    explicit CVectorFileReader(std::vector<char> data) : m_data(std::move(data)) {}
    unsigned long long GetSize() const override { return m_data.size(); }
    bool Read(unsigned long long offset, std::vector<char>& data) override
    {
        if (offset > m_data.size() || data.size() > m_data.size() - offset)
            return false;
        std::copy(m_data.begin() + (size_t)offset, m_data.begin() + (size_t)offset + data.size(), data.begin());
        return true;
    }
};

static std::unique_ptr<orthia::IFileRangeReader> OpenTestFile(const orthia::PlatformString_type& name)
{
    std::vector<char> data;
    if (orthia::LoadFileToVector_Silent(name, data))
        return nullptr;
    return std::make_unique<CVectorFileReader>(std::move(data));
}

static const orthia::FileOpener_type c_openTestFile = OpenTestFile;

static const orthia::ImageSection* FindSection(const orthia::ImageSections& result, const char* name)
{
    auto nameText = orthia::Utf8ToPlatformString(name);
    for (const auto& section : result.sections)
    {
        if (section.name == nameText)
            return &section;
    }
    return nullptr;
}

static std::vector<const orthia::ImageSection*> FindAll(const orthia::ImageSections& result, const char* name)
{
    auto nameText = orthia::Utf8ToPlatformString(name);
    std::vector<const orthia::ImageSection*> found;
    for (const auto& section : result.sections)
    {
        if (section.name == nameText)
            found.push_back(&section);
    }
    return found;
}

static bool ReasonContains(const orthia::ImageSections& result, const char* text)
{
    return orthia::PlatformStringToUtf8(result.reason).find(text) != std::string::npos;
}

// ls.bin is a dump of a running `ls`: the memory image of a PIE, section headers included as
// whatever the process had at e_shoff
static const orthia::Address_type c_dumpBase = 0x7f1234560000ULL;

static void test_sections_process_dump_lists_segments()
{
    orthia::CReaderOverVector reader(c_dumpBase, LoadElfTestFile(ORTHIA_TCSTR("ls.bin")));
    orthia::ImageSections result;
    orthia::QueryImageSections(&reader, c_dumpBase, orthia::PlatformString_type(), c_openTestFile, result);

    DIANA_TEST_ASSERT(result.segments);
    DIANA_TEST_ASSERT(ReasonContains(result, "no file"));
    DIANA_TEST_ASSERT(result.source.empty());

    auto loads = FindAll(result, "LOAD");
    DIANA_TEST_ASSERT(loads.size() == 4);
    if (loads.size() == 4)
    {
        // the lowest PT_LOAD (p_vaddr 0) is at the module base
        DIANA_TEST_ASSERT(loads[0]->address == c_dumpBase && loads[0]->size == 0x36f8);
        DIANA_TEST_ASSERT(loads[0]->flagsShort == ORTHIA_TCSTR("R--"));
        DIANA_TEST_ASSERT(loads[1]->address == c_dumpBase + 0x4000 && loads[1]->size == 0x14f41);
        DIANA_TEST_ASSERT(loads[1]->flagsShort == ORTHIA_TCSTR("R-X"));
        DIANA_TEST_ASSERT(loads[2]->address == c_dumpBase + 0x19000);
        DIANA_TEST_ASSERT(loads[3]->address == c_dumpBase + 0x21f30 && loads[3]->size == 0x25e8);
        DIANA_TEST_ASSERT(loads[3]->flagsShort == ORTHIA_TCSTR("RW-"));
    }
    auto dynamic = FindSection(result, "DYNAMIC");
    DIANA_TEST_ASSERT(dynamic && dynamic->address == c_dumpBase + 0x22a38);
    DIANA_TEST_ASSERT(FindSection(result, "GNU_RELRO"));
    // zero sized
    DIANA_TEST_ASSERT(!FindSection(result, "GNU_STACK"));
}

static void test_sections_process_dump_ignores_garbage_section_headers()
{
    // the dump is also its own "file": the headers match, but e_shoff points into the data segment
    orthia::CReaderOverVector reader(c_dumpBase, LoadElfTestFile(ORTHIA_TCSTR("ls.bin")));
    orthia::ImageSections result;
    orthia::QueryImageSections(&reader, c_dumpBase, ElfTestFilePath(ORTHIA_TCSTR("ls.bin")), c_openTestFile, result);

    DIANA_TEST_ASSERT(result.segments);
    DIANA_TEST_ASSERT(ReasonContains(result, "invalid section headers"));
    DIANA_TEST_ASSERT(FindAll(result, "LOAD").size() == 4);
    DIANA_TEST_ASSERT(result.debugLink.empty());
}

static void test_sections_mapped_file_reads_section_headers_from_disk()
{
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(LoadElfTestFile(ORTHIA_TCSTR("dmesg")), params);
    const auto base = elf.GetImageBase();
    orthia::CReaderOverVector reader(base, elf.GetMappedFile());

    auto path = ElfTestFilePath(ORTHIA_TCSTR("dmesg"));
    orthia::ImageSections result;
    orthia::QueryImageSections(&reader, base, path, c_openTestFile, result);

    DIANA_TEST_ASSERT(!result.segments);
    DIANA_TEST_ASSERT(result.reason.empty());
    DIANA_TEST_ASSERT(result.source == path);
    // 31 headers without the null one
    DIANA_TEST_ASSERT(result.sections.size() == 30);

    auto text = FindSection(result, ".text");
    DIANA_TEST_ASSERT(text && text->address == base + 0x4d80 && text->size == 0x6692);
    DIANA_TEST_ASSERT(text && text->flagsShort == ORTHIA_TCSTR("R-X"));
    auto bss = FindSection(result, ".bss");
    DIANA_TEST_ASSERT(bss && bss->address == base + 0x10920 && bss->flagsShort == ORTHIA_TCSTR("RW-"));
    auto interp = FindSection(result, ".interp");
    DIANA_TEST_ASSERT(interp && interp->address == base + 0x318 && interp->flagsShort == ORTHIA_TCSTR("R--"));
    // not loaded: no address
    auto shstrtab = FindSection(result, ".shstrtab");
    DIANA_TEST_ASSERT(shstrtab && shstrtab->address == 0 && shstrtab->flagsShort == ORTHIA_TCSTR("---"));
    // Ubuntu packaging names the debug file after the build-id
    DIANA_TEST_ASSERT(result.debugLink == ORTHIA_TCSTR("342db32e32e961622c0d19322ad591d56a65a8.debug"));
}

static void test_sections_other_file_is_rejected()
{
    orthia::CSimpleElfFile elf;
    orthia::MapFileParameters params;
    elf.MapFile(LoadElfTestFile(ORTHIA_TCSTR("dmesg")), params);
    const auto base = elf.GetImageBase();
    orthia::CReaderOverVector reader(base, elf.GetMappedFile());

    orthia::ImageSections result;
    orthia::QueryImageSections(&reader, base, ElfTestFilePath(ORTHIA_TCSTR("apt-mark")), c_openTestFile, result);
    DIANA_TEST_ASSERT(result.segments);
    DIANA_TEST_ASSERT(ReasonContains(result, "is not the loaded image"));
    DIANA_TEST_ASSERT(FindAll(result, "LOAD").size() == 4);
    // apt-mark has a debug link too, but it is not the loaded image
    DIANA_TEST_ASSERT(result.debugLink.empty());

    orthia::QueryImageSections(&reader, base, ElfTestFilePath(ORTHIA_TCSTR("no-such-file")), c_openTestFile, result);
    DIANA_TEST_ASSERT(result.segments);
    DIANA_TEST_ASSERT(ReasonContains(result, "can't open"));
}

// A minimal big-endian ELF32 executable: one PT_LOAD, sections null/.text/.shstrtab
static void PutBe(std::vector<char>& image, size_t offset, unsigned long long value, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        image[offset + size - 1 - i] = (char)(value >> (8 * i));
    }
}

static const char c_testDebugLink[] = "elf32be.debug";

static std::vector<char> MakeElf32Be(bool withDebugLink)
{
    const int sectionCount = withDebugLink ? 4 : 3;
    std::vector<char> image(0x200 + sectionCount * 40);
    const char ident[] = { 0x7f, 'E', 'L', 'F', 1 /* ELFCLASS32 */, 2 /* ELFDATA2MSB */, 1 };
    memcpy(image.data(), ident, sizeof(ident));
    PutBe(image, 16, 2, 2);             // e_type: ET_EXEC
    PutBe(image, 18, 8, 2);             // e_machine: MIPS
    PutBe(image, 20, 1, 4);             // e_version
    PutBe(image, 24, 0x10000100, 4);    // e_entry
    PutBe(image, 28, 52, 4);            // e_phoff
    PutBe(image, 32, 0x200, 4);         // e_shoff
    PutBe(image, 40, 52, 2);            // e_ehsize
    PutBe(image, 42, 32, 2);            // e_phentsize
    PutBe(image, 44, 1, 2);             // e_phnum
    PutBe(image, 46, 40, 2);            // e_shentsize
    PutBe(image, 48, sectionCount, 2);  // e_shnum
    PutBe(image, 50, 2, 2);             // e_shstrndx

    PutBe(image, 52 + 0, 1, 4);             // p_type: PT_LOAD
    PutBe(image, 52 + 4, 0, 4);             // p_offset
    PutBe(image, 52 + 8, 0x10000000, 4);    // p_vaddr
    PutBe(image, 52 + 12, 0x10000000, 4);   // p_paddr
    PutBe(image, 52 + 16, 0x200, 4);        // p_filesz
    PutBe(image, 52 + 20, 0x200, 4);        // p_memsz
    PutBe(image, 52 + 24, 5, 4);            // p_flags: R+X
    PutBe(image, 52 + 28, 0x1000, 4);       // p_align

    const char names[] = "\0.text\0.shstrtab\0.gnu_debuglink";
    memcpy(image.data() + 0x180, names, sizeof(names));

    const size_t text = 0x200 + 40;
    PutBe(image, text + 0, 1, 4);           // sh_name
    PutBe(image, text + 4, 1, 4);           // SHT_PROGBITS
    PutBe(image, text + 8, 6, 4);           // SHF_ALLOC | SHF_EXECINSTR
    PutBe(image, text + 12, 0x10000100, 4); // sh_addr
    PutBe(image, text + 16, 0x100, 4);      // sh_offset
    PutBe(image, text + 20, 0x20, 4);       // sh_size
    PutBe(image, text + 32, 4, 4);          // sh_addralign

    const size_t strtab = 0x200 + 80;
    PutBe(image, strtab + 0, 7, 4);         // sh_name
    PutBe(image, strtab + 4, 3, 4);         // SHT_STRTAB
    PutBe(image, strtab + 16, 0x180, 4);    // sh_offset
    PutBe(image, strtab + 20, sizeof(names), 4);
    PutBe(image, strtab + 32, 1, 4);

    if (withDebugLink)
    {
        // the file name, padding to 4 and a CRC
        memcpy(image.data() + 0x1c0, c_testDebugLink, sizeof(c_testDebugLink));
        PutBe(image, 0x1d0, 0x12345678, 4);
        const size_t link = 0x200 + 120;
        PutBe(image, link + 0, 17, 4);      // sh_name
        PutBe(image, link + 4, 1, 4);       // SHT_PROGBITS
        PutBe(image, link + 16, 0x1c0, 4);  // sh_offset
        PutBe(image, link + 20, 0x14, 4);   // sh_size
        PutBe(image, link + 32, 4, 4);
    }
    return image;
}

static void test_sections_elf32_big_endian()
{
    auto image = MakeElf32Be(false);
    const orthia::PlatformString_type path = ORTHIA_TCSTR("elf32be");
    orthia::FileOpener_type openFile = [&](const orthia::PlatformString_type& name)
        -> std::unique_ptr<orthia::IFileRangeReader>
    {
        if (name != path)
            return nullptr;
        return std::make_unique<CVectorFileReader>(image);
    };

    // only the PT_LOAD part is in memory
    const orthia::Address_type base = 0x10000000;
    orthia::CReaderOverVector reader(base, std::vector<char>(image.begin(), image.begin() + 0x200));
    orthia::ImageSections result;
    orthia::QueryImageSections(&reader, base, path, openFile, result);

    DIANA_TEST_ASSERT(!result.segments);
    DIANA_TEST_ASSERT(result.sections.size() == 2);
    auto text = FindSection(result, ".text");
    DIANA_TEST_ASSERT(text && text->address == 0x10000100 && text->size == 0x20 && text->flagsShort == ORTHIA_TCSTR("R-X"));
    auto shstrtab = FindSection(result, ".shstrtab");
    DIANA_TEST_ASSERT(shstrtab && shstrtab->address == 0);
    DIANA_TEST_ASSERT(result.debugLink.empty());

    // no opener: the file can't be read
    orthia::QueryImageSections(&reader, base, path, orthia::FileOpener_type(), result);
    DIANA_TEST_ASSERT(result.segments);
    DIANA_TEST_ASSERT(ReasonContains(result, "can't open"));

    orthia::QueryImageSections(&reader, base, orthia::PlatformString_type(), openFile, result);
    DIANA_TEST_ASSERT(result.segments);
    auto load = FindSection(result, "LOAD");
    DIANA_TEST_ASSERT(load && load->address == base && load->size == 0x200 && load->flagsShort == ORTHIA_TCSTR("R-X"));

    image = MakeElf32Be(true);
    orthia::CReaderOverVector linkedReader(base, std::vector<char>(image.begin(), image.begin() + 0x200));
    orthia::QueryImageSections(&linkedReader, base, path, openFile, result);
    DIANA_TEST_ASSERT(!result.segments);
    DIANA_TEST_ASSERT(result.sections.size() == 3);
    DIANA_TEST_ASSERT(FindSection(result, ".gnu_debuglink"));
    DIANA_TEST_ASSERT(result.debugLink == orthia::Utf8ToPlatformString(c_testDebugLink));
}

void test_sections()
{
    if (ElfTestFilesPresent("test_sections", { ORTHIA_TCSTR("ls.bin"), ORTHIA_TCSTR("dmesg"), ORTHIA_TCSTR("apt-mark") }))
    {
        DIANA_TEST(test_sections_process_dump_lists_segments());
        DIANA_TEST(test_sections_process_dump_ignores_garbage_section_headers());
        DIANA_TEST(test_sections_mapped_file_reads_section_headers_from_disk());
        DIANA_TEST(test_sections_other_file_is_rejected());
    }
    DIANA_TEST(test_sections_elf32_big_endian());
}
