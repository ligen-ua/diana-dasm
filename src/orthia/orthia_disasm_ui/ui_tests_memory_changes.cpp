// A process whose memory changes between the opens: open -> analyze -> .capture -> close -> the
// process changes its memory -> open again -> ... At each step db and u (ReadData), the disassembly
// window (CreateDisasmStream) and the analysis must say what that view has: the process as it is
// now in a live view, the last capture in a saved view (--proc-folder)

#include "ui_tests_fakes.h"
#include "orthia_capture.h"
#include "orthia_image_source.h"
#include "orthia_sha256.h"
#include "orthia_database_module.h"
#include <fstream>
#include <iterator>
#include <set>

using orthia_tests::CFakeProcess;
using orthia_tests::FloatingFixture;
using orthia_tests::PutRel32;

namespace
{
    namespace fs = std::filesystem;
    using F = FloatingFixture;

    // FloatingFixture::process->regions
    const size_t c_regionA = 0;
    const size_t c_regionData = 1;
    const size_t c_regionD = 2;

    // db and u read through ReadData; -1: the view has no byte there
    int ReadByte(orthia::CProcessWorkplaceItem& item, orthia::Address_type address)
    {
        const auto data = item.ReadData(address, 1);
        if ((data.rangeFlags & orthia::WorkAddressData::flags_FullInvalid) || !data.pDataStart || !data.dataSize)
        {
            return -1;
        }
        if (data.pDataFlags && (data.pDataFlags[0] & orthia::WorkAddressData::dataFlags_Invalid))
        {
            return -1;
        }
        return (unsigned char)data.pDataStart[0];
    }

    // the disassembly window reads through CreateDisasmStream
    int ReadDisasmByte(orthia::CProcessWorkplaceItem& item, orthia::Address_type address)
    {
        auto stream = item.CreateDisasmStream(address);
        unsigned char byte = 0;
        if (!stream || DianaExactReadSafe(&stream->parent, &byte, 1) != DI_SUCCESS)
        {
            return -1;
        }
        return byte;
    }

    // what both read paths show at address
    void ExpectByte(orthia::CProcessWorkplaceItem& item, orthia::Address_type address, int expected)
    {
        EXPECT_EQ(expected, ReadByte(item, address)) << std::hex << address;
        EXPECT_EQ(expected, ReadDisasmByte(item, address)) << std::hex << address;
    }

    std::vector<std::pair<orthia::Address_type, orthia::Address_type>> ModuleReferences(F& fixture)
    {
        std::vector<std::pair<orthia::Address_type, orthia::Address_type>> refs;
        fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryAllModuleReferences(F::moduleBase, 0x1000, refs);
        std::sort(refs.begin(), refs.end());
        return refs;
    }

    std::string Describe(const orthia::IWorkPlaceItem& item, orthia::Address_type address)
    {
        std::vector<orthia::PlatformString_type> lines;
        orthia::DescribeFloatingModule(item, F::FindModule(item, address), lines);
        std::string text;
        for (const auto& line : lines)
        {
            text += orthia::PlatformStringToUtf8(line) + "\n";
        }
        return text;
    }

    bool IsStale(const orthia::ModuleInfo& module)
    {
        return (module.flags & orthia::ModuleInfo::flags_stale) != 0;
    }
    bool IsAnalyzed(const orthia::ModuleInfo& module)
    {
        return (module.flags & orthia::ModuleInfo::flags_analyzeDone) != 0;
    }

    // the capture folder holds what its index names, each file whole, and nothing else
    void ExpectCaptureFolderClean(const fs::path& root)
    {
        orthia::CaptureIndex index;
        ASSERT_TRUE(orthia::ReadCaptureIndex(root.native(), index));
        std::set<orthia::PlatformString_type> named;
        for (const auto& region : index.regions)
        {
            if (region.file.empty())
                continue;
            const auto name = orthia::Utf8ToPlatformString(region.file);
            named.insert(name);
            const auto path = root / orthia::g_captureFolder / name;
            ASSERT_TRUE(fs::is_regular_file(path)) << region.file;
            EXPECT_EQ(region.size, fs::file_size(path)) << region.file;
        }
        for (const auto& entry : fs::directory_iterator(root / orthia::g_captureFolder))
        {
            const auto name = entry.path().filename().native();
            if (name == ORTHIA_TCSTR("capture.xml"))
                continue;
            EXPECT_TRUE(named.count(name)) << "left over: " << entry.path().string();
        }
    }

    std::vector<char> ReadFile(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }
}

// Data in the module and in the heap changes; the code doesn't. A live open shows the process as
// it is now and keeps the analysis; a saved view shows the last capture; --capture of the module
// replaces only the module's range in it
TEST(MemoryChanges, ModuleAndHeapData)
{
    FloatingFixture fixture(ORTHIA_TCSTR("orthia_test_memory_data"), true);
    const orthia::Address_type moduleData = F::moduleBase + 0x800;
    auto setData = [&](char inModule, char inHeap) {
        fixture.process->images[0].data[0x800] = inModule;
        fixture.process->regions[c_regionData].data[0] = inHeap;
    };
    setData(0x11, 0x11);

    // live: analyze, db, .capture
    std::vector<std::pair<orthia::Address_type, orthia::Address_type>> refs;
    {
        auto item = fixture.OpenItem();
        fixture.AnalyzeModule(*item);
        ExpectByte(*item, moduleData, 0x11);
        ExpectByte(*item, F::regionData, 0x11);
        refs = ModuleReferences(fixture);
        ASSERT_FALSE(refs.empty());
        orthia::CaptureProcess(*fixture.process, fixture.root.native(), nullptr);
    }

    // closed; the process writes its data
    setData(0x22, 0x33);

    // live again: the process, not the capture; the analysis stays, the data isn't code
    {
        auto item = fixture.Reopen();
        ExpectByte(*item, moduleData, 0x22);
        ExpectByte(*item, F::regionData, 0x33);
        const auto module = F::FindModule(*item, F::moduleBase);
        EXPECT_TRUE(IsAnalyzed(module));
        EXPECT_FALSE(IsStale(module));
        EXPECT_EQ(refs, ModuleReferences(fixture));
    }

    // the saved folder: the capture, as it was
    {
        auto item = fixture.OpenSaved();
        ExpectByte(*item, moduleData, 0x11);
        ExpectByte(*item, F::regionData, 0x11);
        const auto module = F::FindModule(*item, F::moduleBase);
        EXPECT_TRUE(IsAnalyzed(module));
        const auto status = orthia::PlatformStringToUtf8(orthia::ModuleStatusText(module));
        EXPECT_NE(std::string::npos, status.find("captured")) << status;
    }

    // .capture again from a live open: the saved folder follows, no file of the first one is left
    {
        auto item = fixture.Reopen();
        orthia::CaptureProcess(*fixture.process, fixture.root.native(), nullptr);
    }
    {
        auto item = fixture.OpenSaved();
        ExpectByte(*item, moduleData, 0x22);
        ExpectByte(*item, F::regionData, 0x33);
    }
    ExpectCaptureFolderClean(fixture.root);

    // --capture: the module analyzed again (.analyze /f) is captured again when its analysis is
    // done, the heap stays as the last .capture has it
    setData(0x44, 0x55);
    {
        auto item = fixture.Reopen();
        item->SetCaptureModules(true);
        item->UpdateModuleFlags(F::moduleBase, 0, orthia::ModuleInfo::flags_analyzeDone);
        item->UpdateModuleFlags(F::moduleBase, orthia::ModuleInfo::flags_analyzeDone, 0);
        ExpectByte(*item, moduleData, 0x44);
        ExpectByte(*item, F::regionData, 0x55);
    }
    {
        auto item = fixture.OpenSaved();
        ExpectByte(*item, moduleData, 0x44);
        ExpectByte(*item, F::regionData, 0x33);
    }
    ExpectCaptureFolderClean(fixture.root);

    // and the live view still the process
    {
        auto item = fixture.Reopen();
        ExpectByte(*item, F::regionData, 0x55);
        EXPECT_EQ(refs, ModuleReferences(fixture));
    }
}

// Shellcode in floating code changes between the opens (no PE/ELF: raw code in private memory).
// A live open shows the new code and marks its module stale; a saved view shows the capture and
// checks the analysis against it; .analyze /f takes the new code, and the next capture follows
TEST(MemoryChanges, ShellcodeChanges)
{
    FloatingFixture fixture(ORTHIA_TCSTR("orthia_test_memory_shellcode"), true);
    auto region = [&](size_t index) -> std::vector<char>& { return fixture.process->regions[index].data; };

    // live: the module analyzed leads to A, A to D (c3); .capture
    std::string firstSnapshot;
    {
        auto item = fixture.OpenItem();
        fixture.AnalyzeModule(*item);
        orthia::DiscoverFloatingCode(*item, F::FindModule(*item, F::moduleBase));
        F::AnalyzePending(*item);
        const auto d = F::FindModule(*item, F::regionD);
        ASSERT_TRUE(d.IsFloating());
        EXPECT_TRUE(IsAnalyzed(d));
        EXPECT_EQ(F::regionD, d.floating->codeLast);
        firstSnapshot = d.floating->snapshotFile;
        ASSERT_FALSE(firstSnapshot.empty());
        ExpectByte(*item, F::regionD, 0xC3);
        orthia::CaptureProcess(*fixture.process, fixture.root.native(), nullptr);
    }

    // closed; D's code changes: nop, then the ret at +1
    region(c_regionD)[0] = (char)0x90;

    // live again: the new code; D stale and kept with its entry, A not
    {
        auto item = fixture.Reopen();
        ExpectByte(*item, F::regionD, 0x90);
        const auto d = F::FindModule(*item, F::regionD);
        ASSERT_TRUE(d.IsFloating());
        EXPECT_TRUE(IsStale(d));
        ASSERT_EQ(1u, d.floating->entries.size());
        EXPECT_EQ((std::vector<orthia::Address_type>{ F::regionA }), d.floating->entries[0].referrers);
        // the analysis as it was: up to the old ret
        EXPECT_EQ(F::regionD, d.floating->codeLast);
        EXPECT_FALSE(IsStale(F::FindModule(*item, F::regionA)));
        const auto summary = orthia::PlatformStringToUtf8(orthia::FormatFloatingSummary(*item));
        EXPECT_NE(std::string::npos, summary.find(", 1 stale")) << summary;
    }

    // the saved folder: the capture has the code that was analyzed, so D is not stale there
    {
        auto item = fixture.OpenSaved();
        ExpectByte(*item, F::regionD, 0xC3);
        EXPECT_FALSE(IsStale(F::FindModule(*item, F::regionD)));
        const auto text = Describe(*item, F::regionD);
        EXPECT_NE(std::string::npos, text.find("Snapshot: ")) << text;
        EXPECT_NE(std::string::npos, text.find("Capture: ")) << text;
        EXPECT_EQ(std::string::npos, text.find("differs")) << text;
    }

    // live: .analyze /f D takes the new code, a new snapshot; .capture
    std::string secondSnapshot;
    {
        auto item = fixture.Reopen();
        auto d = F::FindModule(*item, F::regionD);
        ASSERT_EQ(orthia::FloatingRefresh::refreshed, orthia::RefreshStaleFloatingModule(*item, d));
        F::AnalyzePending(*item);
        const auto analyzed = F::FindModule(*item, F::regionD);
        EXPECT_TRUE(IsAnalyzed(analyzed));
        EXPECT_FALSE(IsStale(analyzed));
        EXPECT_EQ(F::regionD + 1, analyzed.floating->codeLast);
        secondSnapshot = analyzed.floating->snapshotFile;
        EXPECT_NE(firstSnapshot, secondSnapshot);
        const auto bytes = ReadFile(fixture.root / orthia::g_floatingSnapshotFolder / orthia::Utf8ToPlatformString(secondSnapshot));
        EXPECT_EQ(region(c_regionD), bytes);
        EXPECT_EQ(orthia::Sha256Hex(bytes.data(), bytes.size()), analyzed.floating->sha256);
        orthia::CaptureProcess(*fixture.process, fixture.root.native(), nullptr);
    }

    // live again: nothing stale; saved: the new capture, which matches the new analysis
    {
        auto item = fixture.Reopen();
        const auto d = F::FindModule(*item, F::regionD);
        EXPECT_FALSE(IsStale(d));
        EXPECT_TRUE(IsAnalyzed(d));
        EXPECT_EQ(F::regionD + 1, d.floating->codeLast);
        const auto summary = orthia::PlatformStringToUtf8(orthia::FormatFloatingSummary(*item));
        EXPECT_EQ(std::string::npos, summary.find("stale")) << summary;
    }
    {
        auto item = fixture.OpenSaved();
        ExpectByte(*item, F::regionD, 0x90);
        const auto d = F::FindModule(*item, F::regionD);
        EXPECT_FALSE(IsStale(d));
        EXPECT_EQ(secondSnapshot, d.floating->snapshotFile);
        // the bytes analyzed first are evidence too
        EXPECT_TRUE(fs::exists(fixture.root / orthia::g_floatingSnapshotFolder / orthia::Utf8ToPlatformString(firstSnapshot)));
    }
    ExpectCaptureFolderClean(fixture.root);

    // closed; A now jumps to E, a new allocation, instead of D
    const orthia::Address_type regionE = 0x10500000;
    CFakeProcess::Region e;
    e.base = regionE;
    e.data.assign(0x1000, (char)0xC3);
    fixture.process->regions.push_back(e);
    PutRel32(region(c_regionA), 0, 0xE9, F::regionA, regionE);

    // live: A stale; .analyze /f A finds E
    {
        auto item = fixture.Reopen();
        ExpectByte(*item, F::regionA, 0xE9);
        auto a = F::FindModule(*item, F::regionA);
        EXPECT_TRUE(IsStale(a));
        EXPECT_FALSE(IsStale(F::FindModule(*item, F::regionD)));
        ASSERT_EQ(orthia::FloatingRefresh::refreshed, orthia::RefreshStaleFloatingModule(*item, a));
        F::AnalyzePending(*item);
        EXPECT_FALSE(IsStale(F::FindModule(*item, F::regionA)));
        const auto found = F::FindModule(*item, regionE);
        ASSERT_TRUE(found.IsFloating());
        EXPECT_TRUE(IsAnalyzed(found));
        ASSERT_EQ(1u, found.floating->entries.size());
        EXPECT_EQ((std::vector<orthia::Address_type>{ F::regionA }), found.floating->entries[0].referrers);
        // D: still listed (kept until replaced), with the referrer it was found by
        const auto d = F::FindModule(*item, F::regionD);
        ASSERT_TRUE(d.IsFloating());
        ASSERT_EQ(1u, d.floating->entries.size());
        EXPECT_EQ((std::vector<orthia::Address_type>{ F::regionA }), d.floating->entries[0].referrers);
    }

    // and the next open has all of it as saved
    {
        auto item = fixture.Reopen();
        EXPECT_TRUE(IsAnalyzed(F::FindModule(*item, regionE)));
        EXPECT_FALSE(IsStale(F::FindModule(*item, F::regionA)));
        EXPECT_TRUE(F::FindModule(*item, F::regionD).IsFloating());
    }
}
