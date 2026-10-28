// References between modules: what one module's analysis saves about another, and what stays when
// either of them is analyzed again or unloaded

#include "ui_tests_fakes.h"
#include "orthia_database_module.h"

using orthia_tests::CFakeProcess;
using orthia_tests::FloatingFixture;
using orthia_tests::PutRel32;

namespace
{
    using F = FloatingFixture;
    using RefPair = std::pair<orthia::Address_type, orthia::Address_type>;

    std::vector<orthia::Address_type> RefsTo(F& fixture, orthia::Address_type address)
    {
        std::vector<orthia::CommonReferenceInfo> refs;
        fixture.moduleManager->QueryReferencesToInstruction(address, &refs);
        std::vector<orthia::Address_type> result;
        for (const auto& ref : refs)
        {
            result.push_back(ref.address);
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    std::vector<RefPair> ModuleRefs(F& fixture, orthia::Address_type base, orthia::Address_type size)
    {
        std::vector<RefPair> refs;
        fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryAllModuleReferences(base, size, refs);
        std::sort(refs.begin(), refs.end());
        return refs;
    }

    // the other module of the fixture analyzed as well (it is all ret)
    void AnalyzeOther(F& fixture)
    {
        orthia::ProcessReaderAdapter reader(fixture.process.get());
        fixture.moduleManager->ReloadRange(F::otherModule, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
    }

    std::vector<orthia::ModuleReference> ModuleRefsWithKinds(F& fixture, orthia::Address_type base, orthia::Address_type size)
    {
        std::vector<orthia::ModuleReference> refs;
        fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryAllModuleReferences(base, size, refs);
        return refs;
    }

    // the kind of the row from -> to, -1 when there is none
    int KindOf(const std::vector<orthia::ModuleReference>& refs, orthia::Address_type from, orthia::Address_type to)
    {
        for (const auto& ref : refs)
        {
            if (ref.from == from && ref.to == to)
                return ref.kind;
        }
        return -1;
    }

    void Put32(std::vector<char>& data, size_t offset, unsigned int value)
    {
        memcpy(&data[offset], &value, sizeof(value));
    }
    void Put16(std::vector<char>& data, size_t offset, unsigned short value)
    {
        memcpy(&data[offset], &value, sizeof(value));
    }
    void Put64(std::vector<char>& data, size_t offset, unsigned long long value)
    {
        memcpy(&data[offset], &value, sizeof(value));
    }

    // <3 bytes of opcode> [rip + rel32]: mov rax, [rip+X] is 48 8B 05, lea rcx, [rip+X] 48 8D 0D
    void PutRipRelative(std::vector<char>& data, size_t offset, const char (&opcode)[4], orthia::Address_type base,
        orthia::Address_type target)
    {
        memcpy(&data[offset], opcode, 3);
        Put32(data, offset + 3, (unsigned int)(int)(long long)(target - (base + offset + 7)));
    }

    // mov rcx, imm64: the analysis takes the immediate for an address
    void PutMovRcxImm64(std::vector<char>& data, size_t offset, orthia::Address_type value)
    {
        data[offset] = (char)0x48;
        data[offset + 1] = (char)0xB9;
        Put64(data, offset + 2, value);
    }

    // the image of a PE32+ (AMD64) as the loader maps it: the headers, then .text at 0x1000 with the
    // entry point at its start; code is put at .text offsets
    std::vector<char> MakeMappedPe64(orthia::Address_type imageBase, const std::vector<std::pair<size_t, std::vector<char>>>& code)
    {
        std::vector<char> image(0x2000, 0);
        image[0] = 'M';
        image[1] = 'Z';
        Put32(image, 0x3C, 0x40);                   // e_lfanew
        memcpy(&image[0x40], "PE\0\0", 4);
        Put16(image, 0x44, 0x8664);                 // Machine
        Put16(image, 0x46, 1);                      // NumberOfSections
        Put16(image, 0x54, 0xF0);                   // SizeOfOptionalHeader
        Put16(image, 0x56, 0x0022);                 // executable, large address aware
        const size_t opt = 0x58;
        Put16(image, opt + 0, 0x20B);               // PE32+
        Put32(image, opt + 4, 0x1000);              // SizeOfCode
        Put32(image, opt + 16, 0x1000);             // AddressOfEntryPoint
        Put32(image, opt + 20, 0x1000);             // BaseOfCode
        Put64(image, opt + 24, imageBase);
        Put32(image, opt + 32, 0x1000);             // SectionAlignment
        Put32(image, opt + 36, 0x1000);             // FileAlignment
        Put16(image, opt + 48, 6);                  // MajorSubsystemVersion
        Put32(image, opt + 56, 0x2000);             // SizeOfImage
        Put32(image, opt + 60, 0x1000);             // SizeOfHeaders
        Put16(image, opt + 68, 3);                  // console
        Put32(image, opt + 108, 16);                // NumberOfRvaAndSizes
        const size_t section = opt + 0xF0;
        memcpy(&image[section], ".text\0\0\0", 8);
        Put32(image, section + 8, 0x1000);          // VirtualSize
        Put32(image, section + 12, 0x1000);         // VirtualAddress
        Put32(image, section + 16, 0x1000);         // SizeOfRawData
        Put32(image, section + 20, 0x1000);         // PointerToRawData
        Put32(image, section + 36, 0x60000020);     // code, execute, read
        image[0x1000] = (char)0xC3;                 // the entry point: ret
        for (const auto& part : code)
        {
            memcpy(&image[0x1000 + part.first], part.second.data(), part.second.size());
        }
        return image;
    }
}

// a call from the module into the other module is a reference to the other module's code
TEST(References, ModuleToModule)
{
    F fixture;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);
    AnalyzeOther(fixture);

    EXPECT_EQ((std::vector<orthia::Address_type>{ F::moduleBase + 10 }), RefsTo(fixture, F::otherModule));
    // the other module's own rows are not where the call is from
    for (const auto& ref : ModuleRefs(fixture, F::otherModule, 0x1000))
    {
        EXPECT_NE(F::moduleBase + 10, ref.first);
    }
}

// the references are kept by the module they are made from: the target analyzed again keeps them,
// the source unloaded takes them away
TEST(References, KeptByTheSource)
{
    F fixture;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);
    AnalyzeOther(fixture);

    fixture.moduleManager->ClearModuleReferences(F::otherModule);
    orthia::ProcessReaderAdapter reader(fixture.process.get());
    fixture.moduleManager->AnalyzeRange(F::otherModule, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0,
        std::vector<orthia::Address_type>(), ORTHIA_TCSTR("other"));
    EXPECT_EQ((std::vector<orthia::Address_type>{ F::moduleBase + 10 }), RefsTo(fixture, F::otherModule));

    fixture.moduleManager->UnloadModule(F::moduleBase);
    EXPECT_TRUE(RefsTo(fixture, F::otherModule).empty());
}

// two ranges back to back: the first one's queries and its clear stop at its last byte
TEST(References, AdjacentRanges)
{
    const orthia::Address_type first = 0x20000000;
    const orthia::Address_type second = first + 0x1000;
    const orthia::Address_type target = 0x30000000;

    F fixture;
    fixture.process->images.clear();
    fixture.process->regions.clear();
    for (auto base : { first, second })
    {
        CFakeProcess::Image image;
        image.format = orthia::FindFormatById("raw");
        image.base = base;
        image.data.assign(0x1000, (char)0xC3);
        PutRel32(image.data, 0, 0xE8, base, target);
        fixture.process->images.push_back(image);
    }
    orthia::ProcessReaderAdapter reader(fixture.process.get());
    fixture.moduleManager->ReloadRange(first, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
    fixture.moduleManager->ReloadRange(second, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);

    EXPECT_EQ((std::vector<RefPair>{ { first, target } }), ModuleRefs(fixture, first, 0x1000));
    EXPECT_EQ((std::vector<RefPair>{ { second, target } }), ModuleRefs(fixture, second, 0x1000));

    fixture.moduleManager->ClearModuleReferences(first);
    EXPECT_TRUE(ModuleRefs(fixture, first, 0x1000).empty());
    EXPECT_EQ((std::vector<orthia::Address_type>{ second }), RefsTo(fixture, target));

    fixture.moduleManager->UnloadModule(first);
    EXPECT_EQ((std::vector<orthia::Address_type>{ second }), RefsTo(fixture, target));
}

// what each reference does with its target: a call is code, mov rax, [rip+X] reads data (inside the
// module too), lea and an immediate take an address
TEST(References, Kinds)
{
    const orthia::Address_type base = F::moduleBase;
    F fixture;
    auto& code = fixture.process->images[0].data;
    code.assign(0x1000, (char)0xCC);
    PutRel32(code, 0x00, 0xE8, base, F::otherModule);                       // call other
    PutRipRelative(code, 0x05, "\x48\x8B\x05", base, base + 0x800);         // mov rax, [rip+data]
    PutRipRelative(code, 0x0C, "\x48\x8B\x05", base, F::otherModule + 0x100); // mov rax, [rip+other's data]
    PutRipRelative(code, 0x13, "\x48\x8D\x0D", base, base + 0x40);          // lea rcx, [rip+function]
    PutMovRcxImm64(code, 0x1A, F::otherModule + 0x200);                     // mov rcx, other's function
    code[0x24] = (char)0xC3;
    code[0x40] = (char)0xC3;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);

    const auto refs = ModuleRefsWithKinds(fixture, base, 0x1000);
    EXPECT_EQ(orthia::reference_code, KindOf(refs, base + 0x00, F::otherModule));
    EXPECT_EQ(orthia::reference_data, KindOf(refs, base + 0x05, base + 0x800));
    EXPECT_EQ(orthia::reference_data, KindOf(refs, base + 0x0C, F::otherModule + 0x100));
    EXPECT_EQ(orthia::reference_address, KindOf(refs, base + 0x13, base + 0x40));
    EXPECT_EQ(orthia::reference_address, KindOf(refs, base + 0x1A, F::otherModule + 0x200));

    // the module's own data: xrefs to it, and no route starts there
    std::vector<orthia::CommonReferenceInfo> toData;
    fixture.moduleManager->QueryReferencesToInstruction(base + 0x800, &toData);
    ASSERT_EQ(1u, toData.size());
    EXPECT_EQ(base + 0x05, toData[0].address);
    EXPECT_EQ(orthia::reference_data, toData[0].kind);
    EXPECT_EQ(base + 0x40, fixture.moduleManager->QueryRouteStart(base + 0x900));
}

// a data access into executable memory no module owns is no entry point, a taken address is
TEST(References, FloatingEntriesByKind)
{
    const orthia::Address_type base = F::moduleBase;
    F fixture;
    auto& code = fixture.process->images[0].data;
    code.assign(0x1000, (char)0xCC);
    PutRipRelative(code, 0x00, "\x48\x8B\x05", base, F::regionD + 0x10);    // mov rax, [rip+D]
    PutMovRcxImm64(code, 0x07, F::regionA + 0x30);                          // mov rcx, A
    code[0x11] = (char)0xC3;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);
    orthia::DiscoverFloatingCode(*item, F::FindModule(*item, base));

    EXPECT_FALSE(F::FindModule(*item, F::regionD).IsFloating());
    const auto a = F::FindModule(*item, F::regionA);
    ASSERT_TRUE(a.IsFloating());
    ASSERT_EQ(1u, a.floating->entries.size());
    EXPECT_EQ(F::regionA + 0x30, a.floating->entries[0].address);
    EXPECT_EQ((std::vector<orthia::Address_type>{ base + 0x07 }), a.floating->entries[0].referrers);
    EXPECT_EQ((std::vector<int>{ orthia::reference_address }), a.floating->entries[0].referrerKinds);

    std::vector<orthia::PlatformString_type> lines;
    orthia::DescribeFloatingReferrers(*item, lines);
    ASSERT_EQ(2u, lines.size());
    EXPECT_NE(std::string::npos, orthia::PlatformStringToUtf8(lines[1]).find("[address taken]"))
        << orthia::PlatformStringToUtf8(lines[1]);
}

// the kinds of the referrers are kept in the saved text; the text from before them reads as unknown
TEST(References, FloatingKindsSaved)
{
    orthia::FloatingRegionInfo info;
    info.allocationBase = 0x7ff600010000ULL;
    info.allocationSize = 0x1000;
    info.AddReference(0x7ff600010010ULL, 0x401020, nullptr, orthia::reference_address);
    info.AddReference(0x7ff600010010ULL, 0x401010, nullptr, orthia::reference_code);
    info.AddReference(0x7ff600010010ULL, 0x401030);
    // the kind of a known referrer gets known once
    EXPECT_TRUE(info.AddReference(0x7ff600010010ULL, 0x401030, nullptr, orthia::reference_code));
    EXPECT_FALSE(info.AddReference(0x7ff600010010ULL, 0x401030, nullptr, orthia::reference_address));

    orthia::FloatingRegionInfo parsed;
    ASSERT_TRUE(orthia::ParseFloatingRegion(orthia::FormatFloatingRegion(info), parsed));
    ASSERT_EQ(1u, parsed.entries.size());
    EXPECT_EQ((std::vector<orthia::Address_type>{ 0x401010, 0x401020, 0x401030 }), parsed.entries[0].referrers);
    EXPECT_EQ((std::vector<int>{ orthia::reference_code, orthia::reference_address, orthia::reference_code }),
        parsed.entries[0].referrerKinds);

    auto old = orthia::FormatFloatingRegion(info);
    for (const char* kind : { "/c", "/a" })
    {
        for (size_t pos; (pos = old.find(kind)) != std::string::npos;)
            old.erase(pos, 2);
    }
    ASSERT_TRUE(orthia::ParseFloatingRegion(old, parsed));
    EXPECT_EQ((std::vector<int>{ 0, 0, 0 }), parsed.entries[0].referrerKinds);
}

// a function of a module that only another module calls: found when the module is analyzed with
// the calls saved into it; a data access into it is no entry point
TEST(References, IncomingCodeTargets)
{
    const orthia::Address_type a = 0x10000000;
    const orthia::Address_type b = 0x10200000;
    F fixture;
    fixture.process->images.clear();
    fixture.process->regions.clear();

    CFakeProcess::Image caller;
    caller.format = orthia::FindFormatById("raw");
    caller.base = a;
    caller.data.assign(0x1000, (char)0xCC);
    PutRel32(caller.data, 0, 0xE8, a, b + 0x1100);                           // call b's function
    PutRipRelative(caller.data, 5, "\x48\x8B\x05", a, b + 0x1300);           // mov rax, [rip+b's data]
    caller.data[12] = (char)0xC3;
    fixture.process->images.push_back(caller);

    std::vector<char> function(6, (char)0xC3);                              // call +0x100; ret
    PutRel32(function, 0, 0xE8, b + 0x1100, b + 0x1200);
    CFakeProcess::Image callee;
    callee.format = orthia::FindFormatById("pe");
    callee.base = b;
    callee.name = ORTHIA_TCSTR("callee");
    callee.data = MakeMappedPe64(b, { { 0x100, function }, { 0x200, std::vector<char>(1, (char)0xC3) },
                                      { 0x300, std::vector<char>(1, (char)0xC3) } });
    fixture.process->images.push_back(callee);

    orthia::ProcessReaderAdapter reader(fixture.process.get());
    // nothing calls into it yet: its function is not found
    EXPECT_EQ(0u, fixture.moduleManager->AnalyzeModuleWithIncoming(b, &reader, ORTHIA_TCSTR("callee"), 0));
    EXPECT_EQ(-1, KindOf(ModuleRefsWithKinds(fixture, b, 0x2000), b + 0x1100, b + 0x1200));

    fixture.moduleManager->ReloadRange(a, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
    std::vector<orthia::Address_type> incoming;
    fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryIncomingCodeTargets(b, 0x2000, incoming);
    EXPECT_EQ((std::vector<orthia::Address_type>{ b + 0x1100 }), incoming);

    // .analyze /f: the references from the module go, the analysis starts from the call too
    fixture.moduleManager->ClearModuleReferences(b);
    EXPECT_EQ(1u, fixture.moduleManager->AnalyzeModuleWithIncoming(b, &reader, ORTHIA_TCSTR("callee"), 0));
    const auto refs = ModuleRefsWithKinds(fixture, b, 0x2000);
    EXPECT_EQ(orthia::reference_code, KindOf(refs, b + 0x1100, b + 0x1200));
    // the data is not analyzed as code
    for (const auto& ref : refs)
    {
        EXPECT_NE(b + 0x1300, ref.from);
    }
}

// a database of version 4 (references without a kind) opens: its rows are unknown, the queries work
TEST(References, MigrationFromV4)
{
    namespace fs = std::filesystem;
    const auto path = fs::temp_directory_path() / ORTHIA_TCSTR("orthia_test_references_v4.db");
    std::error_code ec;
    fs::remove(path, ec);
    {
        orthia::CDatabaseManager manager;
        manager.CreateNew(path.native());
    }
    const unsigned long long bias = 0x8000000000000000ULL;
    const orthia::Address_type from = 0x401000, to = 0x402000;
    {
        sqlite3* db = nullptr;
        ASSERT_EQ(SQLITE_OK, sqlite3_open(orthia::PlatformStringToUtf8(path.native()).c_str(), &db));
        const std::string sql =
            "DROP TABLE tbl_references;"
            "CREATE TABLE tbl_references (ref_address_from INTEGER, ref_address_to INTEGER);"
            "CREATE INDEX idx_ref_to ON tbl_references(ref_address_to);"
            "INSERT INTO tbl_references VALUES(" + std::to_string((long long)(from ^ bias)) + ", " +
                std::to_string((long long)(to ^ bias)) + ");"
            "DELETE FROM tbl_version WHERE ver_database_version = 5;";
        EXPECT_EQ(SQLITE_OK, sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr)) << sqlite3_errmsg(db);
        sqlite3_close(db);
    }
    {
        orthia::CModuleManager manager;
        manager.Reinit(path.native(), false);
        std::vector<orthia::CommonReferenceInfo> refs;
        manager.QueryReferencesToInstruction(to, &refs);
        ASSERT_EQ(1u, refs.size());
        EXPECT_EQ(from, refs[0].address);
        EXPECT_EQ(orthia::reference_unknown, refs[0].kind);
        EXPECT_EQ(to, manager.QueryRouteStart(to + 0x10));
    }
    fs::remove(path, ec);
}
