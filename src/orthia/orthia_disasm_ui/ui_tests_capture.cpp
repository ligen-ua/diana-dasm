// .capture and --capture: the memory of a process saved into its folder, and the folder serving it
// after the process exited

#include "ui_tests_fakes.h"
#include "orthia_capture.h"
#include "orthia_saved_process.h"
#include "orthia_item_process.h"
#include "orthia_image_source.h"
#include "orthia_databases.h"
#include <atomic>
#include <fstream>
#include <thread>

using orthia_tests::CFakeProcess;

namespace
{
    namespace fs = std::filesystem;

    struct TempFolder
    {
        fs::path path;
        explicit TempFolder(const ORTHIA_TCHAR* name)
        {
            path = fs::temp_directory_path() / name;
            std::error_code ec;
            fs::remove_all(path, ec);
            fs::create_directories(path);
        }
        ~TempFolder()
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    };

    void WriteFile(const fs::path& path, const std::vector<char>& data)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream.write(data.data(), (std::streamsize)data.size());
    }

    CFakeProcess::Image RawImage(orthia::Address_type base, size_t size, const ORTHIA_TCHAR* name)
    {
        CFakeProcess::Image image;
        image.format = orthia::FindFormatById("raw");
        image.base = base;
        image.data.assign(size, (char)0xC3);
        image.name = name;
        image.fullName = orthia::PlatformString_type(ORTHIA_TCSTR("/lib/")) + name;
        return image;
    }

    CFakeProcess::Region DataRegion(orthia::Address_type base, char fill, int protect)
    {
        CFakeProcess::Region region;
        region.base = base;
        region.data.assign(0x1000, fill);
        region.protect = protect;
        return region;
    }

    // a process folder for OpenSavedProcess: meta.xml, an empty data.db, modules.xml
    void MakeProcessFolder(const fs::path& folder, const orthia::SavedModuleList& list)
    {
        orthia::FolderMeta meta;
        meta.kind = orthia::g_folderKindProcess;
        meta.dianaMode = DIANA_MODE64;
        meta.dianaArch = DIANA_ARCH_X86;
        meta.pid = 7;
        meta.name = ORTHIA_TCSTR("[7] app");
        orthia::WriteFolderMeta((folder / ORTHIA_TCSTR("meta.xml")).native(), meta);
        WriteFile(folder / ORTHIA_TCSTR("data.db"), {});
        orthia::WriteSavedModules(folder.native(), list);
    }

    orthia::CapturedRegion Region(orthia::Address_type base, orthia::Address_type size, const char* file)
    {
        orthia::CapturedRegion region;
        region.base = base;
        region.size = size;
        region.allocationBase = base;
        region.file = file;
        return region;
    }

    const int c_rw = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write;
}

TEST(Capture, IndexRoundTripAndMerge)
{
    orthia::CaptureIndex index;
    index.createdUnix = 1792596612;
    index.complete = false;
    auto region = Region(0x7ff600000000ULL, 0x2000, "7ff600000000_2000.bin");
    region.allocationBase = 0x7ff5ffff0000ULL;
    region.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec;
    region.committed = true;
    region.type = oui::MemoryRegionInfo::type_image;
    region.name = "C:\\dir \"x\"\\a.dll";
    region.missing = 0x1000;
    index.regions.push_back(region);
    index.regions.push_back(Region(0x1000, 0x1000, ""));
    index.hasThreads = true;
    orthia::CapturedThread thread;
    thread.tid = 0x1234;
    thread.startAddress = 0x7ff600001000ULL;
    thread.startTime = "2026-10-27 12:00:00.000";
    thread.stack = { 0x7ff600001010ULL, 0x401000 };
    index.threads.push_back(thread);

    orthia::CaptureIndex parsed;
    ASSERT_TRUE(orthia::ParseCaptureIndex(orthia::FormatCaptureIndex(index), parsed));
    EXPECT_EQ(index.createdUnix, parsed.createdUnix);
    EXPECT_FALSE(parsed.complete);
    ASSERT_EQ(2u, parsed.regions.size());
    // by base
    EXPECT_EQ(0x1000u, parsed.regions[0].base);
    EXPECT_TRUE(parsed.regions[0].file.empty());
    const auto& r = parsed.regions[1];
    EXPECT_EQ(region.base, r.base);
    EXPECT_EQ(region.size, r.size);
    EXPECT_EQ(region.allocationBase, r.allocationBase);
    EXPECT_EQ(region.protect, r.protect);
    EXPECT_TRUE(r.committed);
    EXPECT_EQ(region.type, r.type);
    EXPECT_EQ(region.name, r.name);
    EXPECT_EQ(region.file, r.file);
    EXPECT_EQ(0x1000u, r.missing);
    ASSERT_TRUE(parsed.hasThreads);
    ASSERT_EQ(1u, parsed.threads.size());
    EXPECT_EQ(0x1234u, parsed.threads[0].tid);
    EXPECT_EQ(thread.startTime, parsed.threads[0].startTime);
    EXPECT_EQ(thread.stack, parsed.threads[0].stack);
    EXPECT_FALSE(orthia::ParseCaptureIndex("<x/>", parsed));

    // a newer capture of a range replaces what overlaps it, the rest stays
    std::vector<orthia::CapturedRegion> regions = { Region(0x1000, 0x1000, "a"), Region(0x2000, 0x2000, "b"), Region(0x8000, 0x1000, "c") };
    orthia::MergeCapturedRegions(regions, { Region(0x3000, 0x2000, "d") });
    ASSERT_EQ(3u, regions.size());
    EXPECT_EQ("a", regions[0].file);
    EXPECT_EQ("d", regions[1].file);
    EXPECT_EQ("c", regions[2].file);
}

// .capture of a process, then the folder after it exited: the bytes as they were when captured,
// not as the module file has them; a module captured again (--capture) replaces only its range
TEST(Capture, CapturedMemoryIsServed)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_capture"));
    auto process = std::make_shared<CFakeProcess>();
    process->images.push_back(RawImage(0x10000000, 0x2000, ORTHIA_TCSTR("app")));
    process->images[0].data[0] = (char)0xE9;   // a hook over the first bytes
    process->regions.push_back(DataRegion(0x20000000, 0x11, c_rw));
    // no access: in the map, never read
    process->regions.push_back(DataRegion(0x30000000, 0x22, 0));

    orthia::SavedModuleList list;
    list.mainModule = 0x10000000;
    orthia::SavedModuleInfo app;
    app.module.address = 0x10000000;
    app.module.size = 0x2000;
    app.module.lastValidAddress = 0x10001fff;
    app.module.name = ORTHIA_TCSTR("app");
    app.module.fullName = (folder.path / ORTHIA_TCSTR("no_such_file")).native();
    app.module.builtInFlags = orthia::ModuleInfo::builtInFlags_moduleTypeRaw;
    list.modules.push_back(app);
    MakeProcessFolder(folder.path, list);

    int progressCalls = 0;
    const auto stats = orthia::CaptureProcess(*process, folder.path.native(), [&](const orthia::CaptureStats&) {
        ++progressCalls;
        return true;
    });
    EXPECT_EQ(3u, stats.regions);
    EXPECT_EQ(2u, stats.savedRegions);
    EXPECT_EQ(0x3000u, stats.bytes);
    EXPECT_EQ(0u, stats.missing);
    EXPECT_EQ(2, progressCalls);
    EXPECT_TRUE(orthia::HasCapture(folder.path.native()));
    orthia::CaptureIndex index;
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    EXPECT_TRUE(index.complete);
    EXPECT_TRUE(index.hasThreads);
    ASSERT_EQ(3u, index.regions.size());
    EXPECT_TRUE(index.regions[2].file.empty());

    // the process goes on and changes its memory; the folder has it as it was
    process->images[0].data[0] = (char)0x55;
    process->regions[0].data[0] = (char)0x33;
    auto saved = orthia::OpenSavedProcess(folder.path.native());
    ASSERT_TRUE(saved->HasCapture());
    unsigned char byte = 0;
    EXPECT_EQ(0, saved->ReadExactEx2(0x10000000, &byte, 1));
    EXPECT_EQ(0xE9, byte);
    EXPECT_EQ(0, saved->ReadExactEx2(0x20000000, &byte, 1));
    EXPECT_EQ(0x11, byte);
    EXPECT_NE(0, saved->ReadExactEx2(0x30000000, &byte, 1));
    oui::MemoryRegionInfo info;
    ASSERT_EQ(0, saved->QueryMemoryRegion(0x20000010, info));
    EXPECT_EQ(c_rw, info.protect);
    EXPECT_EQ(0x20000000u, info.allocationBase);
    EXPECT_EQ(0x1000u, info.allocationSize);
    ASSERT_EQ(0, saved->QueryMemoryRegion(0x30000000, info));
    EXPECT_EQ(0, info.protect);
    // the module: captured, though its file is gone
    std::vector<orthia::ModuleInfo> modules;
    int mainOffset = -1;
    saved->QueryModules(modules, mainOffset, [](const orthia::ModuleInfo&, oui::ModuleDisasmContext&) {});
    ASSERT_EQ(1u, modules.size());
    EXPECT_EQ((int)orthia::ImageSourceKind::MemoryImage, modules[0].imageSourceKind);
    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("captured")), orthia::ModuleStatusText(modules[0]));
    // the threads of the capture: none here, but the list is there
    auto threads = saved->CreateThreadEnumerator();
    EXPECT_EQ(0, std::get<0>(threads));
    ASSERT_TRUE(std::get<1>(threads));
    EXPECT_FALSE(std::get<1>(std::get<1>(threads)->GetNextThread()));

    // --capture of the module: its range again, the heap stays as it was
    const auto oldFile = index.regions[0].file;
    orthia::CaptureRange(*process, folder.path.native(), 0x10000000, 0x2000);
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    ASSERT_EQ(3u, index.regions.size());
    saved = orthia::OpenSavedProcess(folder.path.native());
    EXPECT_EQ(0, saved->ReadExactEx2(0x10000000, &byte, 1));
    EXPECT_EQ(0x55, byte);
    EXPECT_EQ(0, saved->ReadExactEx2(0x20000000, &byte, 1));
    EXPECT_EQ(0x11, byte);
    // the same region again: the same file, rewritten
    EXPECT_EQ(oldFile, index.regions[0].file);
    EXPECT_TRUE(fs::exists(folder.path / orthia::g_captureFolder / orthia::Utf8ToPlatformString(oldFile)));
}

TEST(Capture, CancelKeepsWhatIsSaved)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_capture_cancel"));
    auto process = std::make_shared<CFakeProcess>();
    for (int i = 0; i < 3; ++i)
    {
        process->regions.push_back(DataRegion(0x20000000 + i * 0x10000, (char)i, c_rw));
    }
    const auto stats = orthia::CaptureProcess(*process, folder.path.native(), [](const orthia::CaptureStats&) { return false; });
    // the map whole, the bytes of the first region only
    EXPECT_EQ(3u, stats.regions);
    EXPECT_EQ(1u, stats.savedRegions);
    orthia::CaptureIndex index;
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    EXPECT_FALSE(index.complete);
    EXPECT_FALSE(index.hasThreads);
    ASSERT_EQ(3u, index.regions.size());
    EXPECT_FALSE(index.regions[0].file.empty());
    EXPECT_TRUE(index.regions[1].file.empty());
}

// --capture: a module's memory goes into the folder when its analysis is done
TEST(Capture, ModuleCapturedWhenAnalyzed)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_capture_module"));
    auto process = std::make_shared<CFakeProcess>();
    process->images.push_back(RawImage(0x10000000, 0x1000, ORTHIA_TCSTR("app")));
    process->images.push_back(RawImage(0x20000000, 0x1000, ORTHIA_TCSTR("lib")));
    orthia::CProcessWorkplaceItem item(process, process->GetFullFileNameForUI(),
        process->GetDianaMode(), process->GetDianaArch(), nullptr);
    item.SetCaptureModules(true);
    item.ReloadModules();
    item.SetProcFolder(folder.path.native());
    EXPECT_FALSE(orthia::HasCapture(folder.path.native()));

    item.UpdateModuleFlags(0x20000000, orthia::ModuleInfo::flags_analyzeDone, 0);
    orthia::CaptureIndex index;
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    ASSERT_EQ(1u, index.regions.size());
    EXPECT_EQ(0x20000000u, index.regions[0].base);
    EXPECT_FALSE(index.regions[0].file.empty());
    // other flags don't capture
    item.UpdateModuleFlags(0x10000000, orthia::ModuleInfo::flags_symbolsLoaded, 0);
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    EXPECT_EQ(1u, index.regions.size());
}

// a folder with a capture is evidence, as one with floating code snapshots is
TEST(Capture, FolderIsKept)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_capture_kept"));
    const auto procFolder = folder.path / ORTHIA_TCSTR("[99999999] gone 20261021-153012");
    fs::create_directories(procFolder);
    auto process = std::make_shared<CFakeProcess>();
    process->regions.push_back(DataRegion(0x20000000, 0x11, c_rw));
    orthia::CaptureProcess(*process, procFolder.native(), nullptr);
    {
        // the fake process is not running: a folder without evidence would be removed here
        orthia::CProcessWorkplaceItem item(process, process->GetFullFileNameForUI(),
            process->GetDianaMode(), process->GetDianaArch(), nullptr);
        item.SetProcFolder(procFolder.native());
    }
    EXPECT_TRUE(fs::exists(procFolder));
    const auto all = orthia::EnumerateProcDatabases(orthia::AddSlash2(folder.path.native()));
    ASSERT_EQ(1u, all.size());
    EXPECT_TRUE(all[0].hasCapture);
}

// .capture while the analysis runs with --capture: the UI runs commands and the analysis on threads
// of their own, so both can write the same region at once
TEST(Capture, ConcurrentCaptures)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_capture_concurrent"));
    auto process = std::make_shared<CFakeProcess>();
    process->images.push_back(RawImage(0x10000000, 0x40000, ORTHIA_TCSTR("app")));
    for (int i = 0; i < 8; ++i)
    {
        process->regions.push_back(DataRegion(0x20000000 + i * 0x10000, (char)i, c_rw));
    }
    std::atomic<int> errors{ 0 };
    auto full = std::thread([&]() {
        for (int i = 0; i < 30; ++i)
        {
            try { orthia::CaptureProcess(*process, folder.path.native(), nullptr); }
            catch (const std::exception&) { ++errors; }
        }
    });
    auto module = std::thread([&]() {
        for (int i = 0; i < 100; ++i)
        {
            try { orthia::CaptureRange(*process, folder.path.native(), 0x10000000, 0x40000); }
            catch (const std::exception&) { ++errors; }
        }
    });
    full.join();
    module.join();
    EXPECT_EQ(0, errors.load());

    // one index, each file it names whole, no temporary files left
    orthia::CaptureIndex index;
    ASSERT_TRUE(orthia::ReadCaptureIndex(folder.path.native(), index));
    EXPECT_EQ(9u, index.regions.size());
    for (const auto& region : index.regions)
    {
        ASSERT_FALSE(region.file.empty());
        const auto path = folder.path / orthia::g_captureFolder / orthia::Utf8ToPlatformString(region.file);
        EXPECT_EQ(region.size, fs::file_size(path)) << region.file;
    }
    for (const auto& entry : fs::directory_iterator(folder.path / orthia::g_captureFolder))
    {
        EXPECT_NE(ORTHIA_TCSTR(".tmp"), entry.path().extension().native()) << entry.path().string();
    }
}
