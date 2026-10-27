// A process folder opened after the process exited: modules.xml written by the live view,
// CSavedProcess serving what the folder and the module files still have

#include "ui_tests_fakes.h"
#include "orthia_saved_process.h"
#include "orthia_item_process.h"
#include "orthia_image_source.h"
#include "orthia_floating_code.h"
#include "orthia_databases.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
#include <fstream>

using orthia_tests::CFakeProcess;
using orthia_tests::FindTestData;

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

    std::vector<char> ReadFile(const fs::path& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::vector<char>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

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

    orthia::SavedModuleList ReadList(const fs::path& folder)
    {
        orthia::SavedModuleList list;
        EXPECT_TRUE(orthia::ReadSavedModules(folder.native(), list));
        return list;
    }

    std::vector<orthia::PlatformString_type> Names(const orthia::SavedModuleList& list)
    {
        std::vector<orthia::PlatformString_type> names;
        for (const auto& saved : list.modules)
            names.push_back(saved.module.name);
        return names;
    }

    // data/pe/tls_x86_64.dll mapped at base, and the identity of its file
    bool MapTestDll(orthia::Address_type base, std::vector<char>& image, orthia::ImageIdentity& identity, fs::path& path)
    {
        path = FindTestData(ORTHIA_TCSTR("pe/tls_x86_64.dll"));
        if (path.empty())
            return false;
        const auto* format = orthia::FindFormatById("pe");
        orthia::MapFileParameters params;
        params.imageBase = base;
        auto mapped = format->MapFile(ReadFile(path), params);
        image = mapped->GetMappedFile();
        EXPECT_TRUE(format->QueryIdentity(*mapped, identity));
        return true;
    }
}

TEST(SavedProcess, ModuleListRoundTrip)
{
    orthia::SavedModuleList list;
    list.mainModule = 0x400000;
    orthia::SavedModuleInfo pe;
    pe.module.address = 0x400000;
    pe.module.size = 0x5000;
    pe.module.name = ORTHIA_TCSTR("app.exe");
    pe.module.fullName = orthia::Utf8ToPlatformString("C:\\dir \"x\"\\app.exe");
    pe.module.dianaMode = DIANA_MODE32;
    pe.module.dianaArch = DIANA_ARCH_X86;
    pe.module.entryPoint = 0x401234;
    pe.module.builtInFlags = orthia::ModuleInfo::builtInFlags_moduleTypePe;
    pe.identity.format = orthia::ImageIdentity::format_Pe;
    pe.identity.timeDateStamp = 0x5f00aa11;
    pe.identity.sizeOfImage = 0x5000;
    pe.identity.hasGuid = true;
    memset(&pe.identity.guid, 0x5A, sizeof(pe.identity.guid));
    pe.identity.age = 3;
    orthia::SavedModuleInfo elf;
    elf.module.address = 0x7f0000000000ULL;
    elf.module.size = 0x2000;
    elf.module.name = ORTHIA_TCSTR("libc.so.6");
    elf.module.fullName = ORTHIA_TCSTR("/usr/lib/libc.so.6");
    elf.module.dianaMode = DIANA_MODE64;
    elf.identity.format = orthia::ImageIdentity::format_Elf;
    elf.identity.buildIdHex = "0123abcd";
    orthia::SavedModuleInfo none;
    none.module.address = 0x500000;
    none.module.size = 0x1000;
    none.module.name = ORTHIA_TCSTR("raw");
    // out of order: the parser sorts
    list.modules = { elf, pe, none };

    orthia::SavedModuleList parsed;
    ASSERT_TRUE(orthia::ParseSavedModules(orthia::FormatSavedModules(list), parsed));
    EXPECT_EQ(0x400000u, parsed.mainModule);
    ASSERT_EQ(3u, parsed.modules.size());
    const auto& p = parsed.modules[0];
    EXPECT_EQ(0x400000u, p.module.address);
    EXPECT_EQ(0x5000u, p.module.size);
    EXPECT_EQ(0x404fffu, p.module.lastValidAddress);
    EXPECT_EQ(pe.module.fullName, p.module.fullName);
    EXPECT_EQ(DIANA_MODE32, p.module.dianaMode);
    EXPECT_EQ(0x401234u, p.module.entryPoint);
    EXPECT_EQ(int(orthia::ModuleInfo::builtInFlags_moduleTypePe), p.module.builtInFlags);
    EXPECT_TRUE(orthia::SameIdentity(pe.identity, p.identity));
    EXPECT_TRUE(p.identity.hasGuid);
    EXPECT_EQ(3u, p.identity.age);
    EXPECT_FALSE(parsed.modules[1].identity.IsValid());
    EXPECT_EQ("0123abcd", parsed.modules[2].identity.buildIdHex);
    EXPECT_EQ(ORTHIA_TCSTR("libc.so.6"), parsed.modules[2].module.name);

    EXPECT_FALSE(orthia::ParseSavedModules("<x/>", parsed));
    EXPECT_FALSE(orthia::ParseSavedModules("not xml", parsed));
}

// The live view writes modules.xml when it gets the folder and again on each reload: a module
// that loads or unloads while the process runs is in the file the next time
TEST(SavedProcess, ModuleListFollowsReload)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_saved_reload"));
    auto process = std::make_shared<CFakeProcess>();
    process->images.push_back(RawImage(0x10000000, 0x1000, ORTHIA_TCSTR("app")));
    process->images.push_back(RawImage(0x20000000, 0x2000, ORTHIA_TCSTR("a.so")));

    orthia::CProcessWorkplaceItem item(process, process->GetFullFileNameForUI(),
        process->GetDianaMode(), process->GetDianaArch(), nullptr);
    item.ReloadModules();
    // no folder yet: nothing written
    EXPECT_FALSE(fs::exists(folder.path / orthia::g_savedModulesFileName));

    item.SetProcFolder(folder.path.native());
    auto list = ReadList(folder.path);
    EXPECT_EQ((std::vector<orthia::PlatformString_type>{ ORTHIA_TCSTR("app"), ORTHIA_TCSTR("a.so") }), Names(list));
    EXPECT_EQ(0x10000000u, list.mainModule);
    ASSERT_EQ(2u, list.modules.size());
    EXPECT_EQ(0x2000u, list.modules[1].module.size);
    EXPECT_EQ(ORTHIA_TCSTR("/lib/a.so"), list.modules[1].module.fullName);
    EXPECT_EQ(int(orthia::ModuleInfo::builtInFlags_moduleTypeRaw), list.modules[1].module.builtInFlags);

    // a.so unloads, b.so loads where it was, bigger
    process->images.pop_back();
    process->images.push_back(RawImage(0x20000000, 0x3000, ORTHIA_TCSTR("b.so")));
    item.ReloadModules();
    list = ReadList(folder.path);
    EXPECT_EQ((std::vector<orthia::PlatformString_type>{ ORTHIA_TCSTR("app"), ORTHIA_TCSTR("b.so") }), Names(list));
    EXPECT_EQ(0x3000u, list.modules[1].module.size);

    // one more module, and the main one stays
    process->images.push_back(RawImage(0x08000000, 0x1000, ORTHIA_TCSTR("low.so")));
    item.ReloadModules();
    list = ReadList(folder.path);
    EXPECT_EQ((std::vector<orthia::PlatformString_type>{ ORTHIA_TCSTR("low.so"), ORTHIA_TCSTR("app"), ORTHIA_TCSTR("b.so") }),
        Names(list));
    EXPECT_EQ(0x10000000u, list.mainModule);

    // nothing changed: the file isn't written again
    const auto before = fs::last_write_time(folder.path / orthia::g_savedModulesFileName);
    WriteFile(folder.path / ORTHIA_TCSTR("marker"), { 'x' });
    item.ReloadModules();
    EXPECT_EQ(before, fs::last_write_time(folder.path / orthia::g_savedModulesFileName));
}

// The database follows the module list of a live process: exe -> a.so loads -> it unloads -> it
// loads again -> b.so takes its place. An unloaded module keeps its analysis (evidence, and reused
// when it comes back); a module over its place gets none of it
TEST(SavedProcess, DatabaseFollowsModuleChanges)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_saved_sync"));
    auto moduleManager = std::make_shared<orthia::CModuleManager>();
    moduleManager->Reinit((folder.path / ORTHIA_TCSTR("data.db")).native(), true);
    auto database = moduleManager->QueryDatabaseManager()->GetClassicDatabase();

    const orthia::Address_type exeBase = 0x10000000;
    const orthia::Address_type dllBase = 0x20000000;
    // call <exe+100h>; ret: a reference out of the module, which the analysis saves
    auto codeImage = [&](orthia::Address_type base, const ORTHIA_TCHAR* name) {
        auto image = RawImage(base, 0x1000, name);
        image.data[0] = (char)0xE8;
        const int rel = (int)(long long)(exeBase + 0x100 - (base + 5));
        memcpy(&image.data[1], &rel, sizeof(rel));
        image.data[5] = (char)0xC3;
        return image;
    };
    auto process = std::make_shared<CFakeProcess>();
    process->images.push_back(codeImage(exeBase, ORTHIA_TCSTR("app")));

    orthia::CProcessWorkplaceItem item(process, process->GetFullFileNameForUI(),
        process->GetDianaMode(), process->GetDianaArch(), nullptr);
    item.ReloadModules();
    item.Init(moduleManager, nullptr, nullptr);
    item.SetProcFolder(folder.path.native());

    // size: what the analysis maps, which may be less than the OS reports (a PE ends with its last section)
    auto analyze = [&](orthia::Address_type base, orthia::Address_type size = 0x1000) {
        orthia::ProcessReaderAdapter reader(process.get());
        moduleManager->ReloadRange(base, size, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
        item.UpdateModuleFlags(base, orthia::ModuleInfo::flags_analyzeDone, 0);
    };
    auto rows = [&]() {
        std::vector<orthia::CommonModuleInfo> modules;
        moduleManager->QueryLoadedModules(&modules);
        std::set<orthia::Address_type> addresses;
        for (const auto& mod : modules)
            addresses.insert(mod.address);
        return addresses;
    };
    auto references = [&](orthia::Address_type base) {
        std::vector<std::pair<orthia::Address_type, orthia::Address_type>> refs;
        database->QueryAllModuleReferences(base, 0x1000, refs);
        return std::count_if(refs.begin(), refs.end(),
            [&](const std::pair<orthia::Address_type, orthia::Address_type>& ref) { return ref.first - base < 0x1000; });
    };
    // -1: no such module in the list
    auto analyzed = [&](orthia::Address_type base) {
        std::vector<orthia::ModuleInfo> modules;
        item.GetModules(modules);
        for (const auto& mod : modules)
        {
            if (mod.address == base)
                return (mod.flags & orthia::ModuleInfo::flags_analyzeDone) ? 1 : 0;
        }
        return -1;
    };
    const std::set<orthia::Address_type> both = { exeBase, dllBase };
    const std::set<orthia::Address_type> exeOnly = { exeBase };

    // just the exe
    analyze(exeBase);
    EXPECT_EQ(exeOnly, rows());
    EXPECT_EQ(1, analyzed(exeBase));
    EXPECT_GT(references(exeBase), 0);

    // a.so loads: not analyzed until it is
    process->images.push_back(codeImage(dllBase, ORTHIA_TCSTR("a.so")));
    item.ReloadModules();
    EXPECT_EQ(0, analyzed(dllBase));
    analyze(dllBase);
    EXPECT_EQ(both, rows());
    EXPECT_EQ(1, analyzed(dllBase));
    EXPECT_GT(references(dllBase), 0);

    // a.so unloads: its analysis stays in the database
    process->images.pop_back();
    item.ReloadModules();
    EXPECT_EQ(-1, analyzed(dllBase));
    EXPECT_EQ(both, rows());
    EXPECT_GT(references(dllBase), 0);
    EXPECT_EQ(1, analyzed(exeBase));

    // the same a.so comes back where it was: analyzed, from the database
    process->images.push_back(codeImage(dllBase, ORTHIA_TCSTR("a.so")));
    item.ReloadModules();
    EXPECT_EQ(1, analyzed(dllBase));
    EXPECT_EQ(both, rows());

    // b.so takes its place in one reload: none of a.so's analysis is b.so's
    process->images.back() = codeImage(dllBase, ORTHIA_TCSTR("b.so"));
    item.ReloadModules();
    EXPECT_EQ(0, analyzed(dllBase));
    EXPECT_EQ(exeOnly, rows());
    EXPECT_EQ(0, references(dllBase));
    // and b.so is analyzed as a new module
    analyze(dllBase);
    EXPECT_EQ(both, rows());
    EXPECT_EQ(1, analyzed(dllBase));

    // b.so unloads, a bigger c.so covers its place from below: dropped too
    process->images.pop_back();
    item.ReloadModules();
    EXPECT_EQ(both, rows());
    auto big = RawImage(dllBase - 0x1000, 0x3000, ORTHIA_TCSTR("c.so"));
    process->images.push_back(big);
    item.ReloadModules();
    EXPECT_EQ(exeOnly, rows());
    EXPECT_EQ(0, analyzed(dllBase - 0x1000));

    // a module whose analysis mapped less or more than the OS reports is still the same module
    const orthia::Address_type shortBase = 0x30000000;
    process->images.push_back(RawImage(shortBase, 0x2000, ORTHIA_TCSTR("d.so")));
    item.ReloadModules();
    analyze(shortBase, 0x1800);
    item.ReloadModules();
    EXPECT_EQ(1, analyzed(shortBase));
    EXPECT_TRUE(rows().count(shortBase));
    process->images.pop_back();
    item.ReloadModules();
    // and more (ELF: up to the end of bss), next to another module
    const orthia::Address_type longBase = 0x40000000;
    process->images.push_back(RawImage(longBase, 0x1000, ORTHIA_TCSTR("e.so")));
    process->images.push_back(RawImage(longBase + 0x1000, 0x1000, ORTHIA_TCSTR("f.so")));
    item.ReloadModules();
    analyze(longBase, 0x1800);
    item.ReloadModules();
    EXPECT_EQ(1, analyzed(longBase));
    EXPECT_TRUE(rows().count(longBase));
    process->images.pop_back();
    process->images.pop_back();
    item.ReloadModules();

    // the exe was never touched
    EXPECT_EQ(1, analyzed(exeBase));
    EXPECT_GT(references(exeBase), 0);

    // a reopen of the same database: the saved flags come back for what is still the same
    orthia::CProcessWorkplaceItem reopened(process, process->GetFullFileNameForUI(),
        process->GetDianaMode(), process->GetDianaArch(), nullptr);
    reopened.ReloadModules();
    reopened.Init(moduleManager, nullptr, nullptr);
    reopened.LoadSavedModuleFlags();
    std::vector<orthia::ModuleInfo> modules;
    reopened.GetModules(modules);
    ASSERT_EQ(2u, modules.size());
    EXPECT_TRUE(modules[0].flags & orthia::ModuleInfo::flags_analyzeDone);
    EXPECT_FALSE(modules[1].flags & orthia::ModuleInfo::flags_analyzeDone);
}

// what tells the build of a module's file comes from its headers in memory
TEST(SavedProcess, ModuleListHasIdentity)
{
    std::vector<char> image;
    orthia::ImageIdentity fileIdentity;
    fs::path path;
    if (!MapTestDll(0x10000000, image, fileIdentity, path))
        GTEST_SKIP() << "data/ not found";
    TempFolder folder(ORTHIA_TCSTR("orthia_test_saved_identity"));
    auto process = std::make_shared<CFakeProcess>();
    CFakeProcess::Image dll;
    dll.format = orthia::FindFormatById("pe");
    dll.base = 0x10000000;
    dll.data = image;
    dll.name = ORTHIA_TCSTR("tls_x86_64.dll");
    dll.fullName = path.native();
    process->images.push_back(dll);

    orthia::CProcessWorkplaceItem item(process, process->GetFullFileNameForUI(),
        process->GetDianaMode(), process->GetDianaArch(), nullptr);
    item.ReloadModules();
    item.SetProcFolder(folder.path.native());
    auto list = ReadList(folder.path);
    ASSERT_EQ(1u, list.modules.size());
    // the headers have no debug GUID: the file's identity is the same build
    EXPECT_TRUE(orthia::SameIdentity(list.modules[0].identity, fileIdentity))
        << orthia::DescribeIdentityMismatch(list.modules[0].identity, fileIdentity);
    EXPECT_EQ(int(orthia::ModuleInfo::builtInFlags_moduleTypePe), list.modules[0].module.builtInFlags);
}

// a folder: the module file that is still the same build, one that is gone, one that nothing
// identifies; the floating code from its snapshot
TEST(SavedProcess, ServesFilesAndSnapshots)
{
    std::vector<char> image;
    orthia::ImageIdentity identity;
    fs::path dllPath;
    if (!MapTestDll(0x10000000, image, identity, dllPath))
        GTEST_SKIP() << "data/ not found";
    TempFolder folder(ORTHIA_TCSTR("orthia_test_saved_open"));

    orthia::FolderMeta meta;
    meta.kind = orthia::g_folderKindProcess;
    meta.dianaMode = DIANA_MODE64;
    meta.dianaArch = DIANA_ARCH_X86;
    meta.pid = 42;
    meta.name = ORTHIA_TCSTR("[42] app.exe");
    meta.startUnix = 1792596612;
    meta.startKey = "key";
    orthia::WriteFolderMeta((folder.path / ORTHIA_TCSTR("meta.xml")).native(), meta);
    WriteFile(folder.path / ORTHIA_TCSTR("data.db"), {});

    orthia::SavedModuleList list;
    list.mainModule = 0x10000000;
    orthia::SavedModuleInfo dll;
    dll.module.address = 0x10000000;
    dll.module.size = image.size();
    dll.module.name = ORTHIA_TCSTR("tls_x86_64.dll");
    dll.module.fullName = dllPath.native();
    dll.module.dianaMode = DIANA_MODE64;
    dll.module.dianaArch = DIANA_ARCH_X86;
    dll.module.entryPoint = 0x10001000;
    dll.module.builtInFlags = orthia::ModuleInfo::builtInFlags_moduleTypePe;
    // the PE headers alone: no GUID, as the live view saves it
    dll.identity = identity;
    dll.identity.hasGuid = false;
    orthia::SavedModuleInfo gone = dll;
    gone.module.address = 0x20000000;
    gone.module.name = ORTHIA_TCSTR("gone.dll");
    gone.module.fullName = (folder.path / ORTHIA_TCSTR("no_such_file.dll")).native();
    orthia::SavedModuleInfo unknown = dll;
    unknown.module.address = 0x30000000;
    unknown.module.name = ORTHIA_TCSTR("unknown.dll");
    unknown.identity = orthia::ImageIdentity();
    list.modules = { dll, gone, unknown };
    orthia::WriteSavedModules(folder.path.native(), list);

    // a floating module and its snapshot
    fs::create_directories(folder.path / orthia::g_floatingSnapshotFolder);
    std::vector<char> code(0x1000, (char)0x90);
    code[0] = (char)0xC3;
    WriteFile(folder.path / orthia::g_floatingSnapshotFolder / ORTHIA_TCSTR("40000000_x.bin"), code);
    orthia::FloatingRegionInfo region;
    region.allocationBase = 0x40000000;
    region.allocationSize = 0x1000;
    region.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec;
    region.snapshotFile = "40000000_x.bin";

    auto saved = orthia::OpenSavedProcess(folder.path.native());
    ASSERT_TRUE(saved);
    EXPECT_FALSE(saved->IsRunning());
    EXPECT_EQ("key", saved->GetStartInfo().key);
    EXPECT_EQ(DIANA_MODE64, saved->GetDianaMode());
    EXPECT_TRUE(saved->AddSnapshot(region));

    std::vector<orthia::ModuleInfo> modules;
    int mainOffset = -1;
    int callbacks = 0;
    saved->QueryModules(modules, mainOffset, [&](const orthia::ModuleInfo&, oui::ModuleDisasmContext& context) {
        EXPECT_NE(nullptr, context.image);
        ++callbacks;
    });
    ASSERT_EQ(3u, modules.size());
    EXPECT_EQ(0, mainOffset);
    EXPECT_EQ(1, callbacks);
    EXPECT_EQ((int)orthia::ImageSourceKind::LinkedFile, modules[0].imageSourceKind);
    EXPECT_EQ((int)orthia::ImageState::Ready, modules[0].imageState);
    EXPECT_EQ((int)orthia::ImageState::Stale, modules[1].imageState);
    EXPECT_EQ((int)orthia::ImageSourceKind::None, modules[2].imageSourceKind);

    // the module from its file, relocated where it was
    std::vector<char> bytes(image.size());
    EXPECT_EQ(0, saved->ReadExactEx2(0x10000000, bytes.data(), bytes.size()));
    EXPECT_EQ(image, bytes);
    // the floating code from its snapshot
    unsigned char first = 0;
    EXPECT_EQ(0, saved->ReadExactEx2(0x40000000, &first, 1));
    EXPECT_EQ(0xC3, first);
    oui::MemoryRegionInfo info;
    ASSERT_EQ(0, saved->QueryMemoryRegion(0x40000010, info));
    EXPECT_EQ(0x40000000u, info.allocationBase);
    EXPECT_EQ(region.protect, info.protect);
    // the gone module and the rest: nothing
    EXPECT_NE(0, saved->ReadExactEx2(0x20000000, bytes.data(), 16));
    EXPECT_NE(0, saved->ReadExactEx2(0x50000000, bytes.data(), 16));
    EXPECT_NE(0, saved->QueryMemoryRegion(0x50000000, info));

    // an item over it: names from the file, and the folder stays when it closes
    {
        orthia::CProcessWorkplaceItem item(saved, saved->GetFullFileNameForUI(), saved->GetDianaMode(), saved->GetDianaArch(), nullptr);
        item.SetSavedView();
        item.ReloadModules();
        item.SetProcFolder(folder.path.native());
        EXPECT_EQ(0x10001000u, item.GetMainModuleAddress());
        EXPECT_NE(0u, item.QueryAddressByName(oui::String(ORTHIA_TCSTR("tls_x86_64.dll!tls_reasons")), 0));
    }
    EXPECT_TRUE(fs::exists(folder.path / ORTHIA_TCSTR("data.db")));
    // and modules.xml is as it was: a saved view doesn't write it
    EXPECT_EQ(orthia::FormatSavedModules(list), orthia::FormatSavedModules(ReadList(folder.path)));
}

TEST(SavedProcess, ResolveFolder)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_saved_resolve"));
    const auto procFolder = folder.path / ORTHIA_TCSTR("[77] app.exe 20261021-153012");
    fs::create_directories(procFolder);
    WriteFile(procFolder / ORTHIA_TCSTR("data.db"), {});

    std::vector<orthia::DatabaseInfo> all;
    orthia::DatabaseInfo info;
    info.kind = orthia::DatabaseInfo::Kind::Process;
    info.folder = procFolder.native();
    info.folderName = procFolder.filename().native();
    info.pid = 77;
    info.hasDataDb = true;
    all.push_back(info);

    EXPECT_EQ(procFolder.native(), orthia::ResolveSavedProcessFolder(all, procFolder.native()));
    EXPECT_EQ(procFolder.native(), orthia::ResolveSavedProcessFolder(all, (procFolder / ORTHIA_TCSTR("data.db")).native()));
    EXPECT_EQ(procFolder.native(), orthia::ResolveSavedProcessFolder(all, ORTHIA_TCSTR("77")));
    EXPECT_THROW(orthia::ResolveSavedProcessFolder(all, ORTHIA_TCSTR("78")), std::exception);
    EXPECT_THROW(orthia::OpenSavedProcess(folder.path.native()), std::exception);
}

// A second open of the same process finds its folder by the pid and the exact start, also when the
// start in seconds came out a second apart (Linux: from the boot time, and the clock was set)
TEST(SavedProcess, FolderFoundByStart)
{
    TempFolder folder(ORTHIA_TCSTR("orthia_test_proc_by_start"));
    auto make = [&](const ORTHIA_TCHAR* name, unsigned long long pid, const char* key) {
        const auto path = folder.path / name;
        fs::create_directories(path);
        WriteFile(path / ORTHIA_TCSTR("data.db"), {});
        orthia::FolderMeta meta;
        meta.kind = orthia::g_folderKindProcess;
        meta.pid = pid;
        meta.startKey = key;
        orthia::WriteFolderMeta((path / ORTHIA_TCSTR("meta.xml")).native(), meta);
        return path.native();
    };
    const auto first = make(ORTHIA_TCSTR("[5] app 20261027-170940"), 5, "boot:100");
    make(ORTHIA_TCSTR("[5] app 20261027-180000"), 5, "boot:900");
    make(ORTHIA_TCSTR("[6] app 20261027-170941"), 6, "boot:100");
    const auto procFolder = orthia::AddSlash2(folder.path.native());
    EXPECT_EQ(first, orthia::FindProcFolderByStart(procFolder, 5, "boot:100"));
    EXPECT_EQ(orthia::PlatformString_type(), orthia::FindProcFolderByStart(procFolder, 5, "boot:200"));
    EXPECT_EQ(orthia::PlatformString_type(), orthia::FindProcFolderByStart(procFolder, 7, "boot:100"));
    EXPECT_EQ(orthia::PlatformString_type(), orthia::FindProcFolderByStart(procFolder, 5, ""));
}
