#include "gtest/gtest.h"
#include "oui_containers.h"
#include "orthia_utils.h"
#include "oui_layouts_calc.h"
#include "orthia_databases.h"
#include "orthia_common_format.h"
#include "orthia_module_names.h"
#include "orthia_config.h"
#include "orthia_external_symbols.h"
#include "formats/orthia_image_format.h"
#include "orthia_item_process.h"
#include "orthia_process_adapter.h"
#include "orthia_image_source.h"
#include "orthia_floating_code.h"
#include "orthia_sha256.h"
#include "orthia_module_manager.h"
#include "orthia_database_module.h"
extern "C"
{
#include "diana_core.h"
}
#ifndef _WIN32
#include "oui_privileges_posix.h"
#endif
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

static void TestDummyIterators()
{
    oui::CLayoutIterator iterator;
    EXPECT_FALSE(iterator.MoveNext());
    EXPECT_FALSE(iterator.MovePrev());
}

static bool TestForwardIterator(std::shared_ptr<oui::CPanelCommonContext> ctx, std::vector<oui::String::string_type> value)
{
    std::vector<oui::String::string_type> tags;
    oui::CLayoutIterator iterator;
    iterator.InitStart(ctx->GetRootLayout());
    for (; iterator.MoveNext();)
    {
        auto layout = iterator.GetLayout();
        tags.push_back(layout->group->GetTag().native);
    }

    EXPECT_EQ(tags, value);
    return tags == value;
}
static bool TestBackwardIterator(std::shared_ptr<oui::CPanelCommonContext> ctx, std::vector<oui::String::string_type> value)
{
    std::vector<oui::String::string_type> rtags;
    oui::CLayoutIterator iterator;
    iterator.InitEnd(ctx->GetRootLayout());
    for (; iterator.MovePrev();)
    {
        auto layout = iterator.GetLayout();
        rtags.push_back(layout->group->GetTag().native);
    } 

    EXPECT_EQ(rtags, value);
    return rtags == value;
}
static std::vector<std::shared_ptr<oui::PanelLayout>> QueryLayouts(std::shared_ptr<oui::CPanelCommonContext> ctx)
{
    std::vector<std::shared_ptr<oui::PanelLayout>> result;
    result.reserve(20);
    oui::CLayoutIterator iterator;
    iterator.InitStart(ctx->GetRootLayout());
    for (; iterator.MoveNext();)
    {
        auto layout = iterator.GetLayout();
        result.push_back(layout);
    }
    return result;
}
TEST(Layouts, IteratorAndCalc)
{
    // [ ----------- top1---------- ] 
    // [ ------ top2 ---- ] [ right ]
    // [ left ] [ default ] [ right ]
    // [ left ] [ default ] [ right ]
    // [ ---- bottom ---- ] [ right ]

    const oui::Rect top1     { {    1,   2 }, { 2000,  10 } };
    const oui::Rect top2     { {    1,  12 }, { 1980,  10 } };
    const oui::Rect left     { {    1,  22 }, {   20, 970 } };
    const oui::Rect defaultG { {   21,  22 }, { 1960, 970 } };
    const oui::Rect bottom   { {    1, 992 }, { 1980,  10 } };
    const oui::Rect right    { { 1981,  12 }, {   20, 990 } };

    const oui::Rect wndRect{ {1,2}, {2000, 1000} };
    const oui::Rect* expectedRects[] = { &top1, &top2, &left, &defaultG, &bottom, &right };

    auto container = std::make_shared<oui::CPanelContainerWindow>();
    container->CreateDefaultGroup()->SetTag(ORTHIA_TCSTR("Default"));
    container->AttachNewGroup(nullptr, oui::GroupLocation::Left, oui::GroupAttachMode::Sibling)->SetTag(ORTHIA_TCSTR("Left"));
    container->AttachNewGroup(nullptr, oui::GroupLocation::Bottom, oui::GroupAttachMode::Sibling)->SetTag(ORTHIA_TCSTR("Bottom"));
    container->AttachNewGroup(nullptr, oui::GroupLocation::Top, oui::GroupAttachMode::Sibling)->SetTag(ORTHIA_TCSTR("Top2"));
    container->AttachNewGroup(nullptr, oui::GroupLocation::Right, oui::GroupAttachMode::Sibling)->SetTag(ORTHIA_TCSTR("Right"));
    container->AttachNewGroup(nullptr, oui::GroupLocation::Top, oui::GroupAttachMode::Sibling)->SetTag(ORTHIA_TCSTR("Top1"));

    auto ctx = container->GetCommonContext();

    TestDummyIterators();
    // simple tests
    if (!TestForwardIterator(ctx,
        {
            OUI_STR("Top1"), OUI_STR("Top2"), OUI_STR("Left"), OUI_STR("Default"), OUI_STR("Bottom"), OUI_STR("Right")
        }
    ))
    {
        return;
    }
    if (!TestBackwardIterator(ctx,
        {
            OUI_STR("Right"), OUI_STR("Bottom"), OUI_STR("Default"), OUI_STR("Left"), OUI_STR("Top2"),  OUI_STR("Top1")
        }
    ))
    {
        return;
    }

    // do calculation test
    auto rootLayout = ctx->GetRootLayout();
    oui::RepositionLayout(rootLayout, wndRect, true, true);

    // compare layouts with expected result
    auto allLayouts = QueryLayouts(ctx);
    const oui::Rect** currentExp = expectedRects;
    for (auto layout : allLayouts)
    {
        auto& expected = **currentExp;
        EXPECT_EQ(expected.position, layout->rect.position);
        EXPECT_EQ(expected.size, layout->rect.size);

        auto groupPosition = layout->group->GetPosition();
        auto groupSize = layout->group->GetSize();
        EXPECT_EQ(expected.position, groupPosition);
        EXPECT_EQ(expected.size, groupSize);
        ++currentExp;
    }
}

static orthia::DatabaseInfo MakeFileDatabase(const orthia::PlatformString_type& sha1)
{
    orthia::DatabaseInfo info;
    info.kind = orthia::DatabaseInfo::Kind::File;
    info.folderName = sha1;
    info.folder = ORTHIA_TCSTR("db/") + sha1;
    info.hasDataDb = true;
    info.lastWrite = std::filesystem::file_time_type::clock::now();
    return info;
}
static orthia::DatabaseInfo MakeProcDatabase(const orthia::PlatformString_type& folderName)
{
    orthia::DatabaseInfo info;
    info.kind = orthia::DatabaseInfo::Kind::Process;
    info.folderName = folderName;
    info.folder = ORTHIA_TCSTR("proc/") + folderName;
    info.pid = orthia::ParseProcFolderPid(folderName);
    info.hasDataDb = true;
    info.lastWrite = std::filesystem::file_time_type::clock::now();
    return info;
}

TEST(Databases, ParseProcFolderPid)
{
    EXPECT_EQ(1234ULL, orthia::ParseProcFolderPid(ORTHIA_TCSTR("[1234] notepad.exe")));
    EXPECT_EQ(0ULL, orthia::ParseProcFolderPid(ORTHIA_TCSTR("notepad.exe")));
    EXPECT_EQ(0ULL, orthia::ParseProcFolderPid(ORTHIA_TCSTR("[12a] x")));
    EXPECT_EQ(0ULL, orthia::ParseProcFolderPid(ORTHIA_TCSTR("[]")));
    EXPECT_EQ(1234ULL, orthia::ParseProcFolderPid(ORTHIA_TCSTR("[1234] notepad.exe 20261021-153012")));
}

TEST(Databases, ProcFolderName)
{
    EXPECT_EQ(orthia::PlatformString_type(), orthia::FormatProcStartStamp(0));
    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("19700101-000001")), orthia::FormatProcStartStamp(1));
    // 2000-02-29: a leap day of a year divisible by 400
    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("20000229-235959")), orthia::FormatProcStartStamp(951868799));
    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("20261021-153012")), orthia::FormatProcStartStamp(1792596612));

    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("[1234] notepad.exe 20261021-153012")),
        orthia::MakeProcFolderName(ORTHIA_TCSTR("[1234] notepad.exe"), 1792596612));
    // the start unknown: named as before meta.xml
    EXPECT_EQ(orthia::PlatformString_type(ORTHIA_TCSTR("[1234] notepad.exe")),
        orthia::MakeProcFolderName(ORTHIA_TCSTR("[1234] notepad.exe"), 0));
}

TEST(Databases, FolderMetaRoundTrip)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / ORTHIA_TCSTR("orthia_test_folder_meta");
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);
    oui::ScopedGuard cleanup([&]() { std::error_code ec2; fs::remove_all(root, ec2); });
    const auto fileName = (root / ORTHIA_TCSTR("meta.xml")).native();

    orthia::FolderMeta meta;
    EXPECT_FALSE(orthia::ReadFolderMeta(fileName, meta));

    orthia::FolderMeta process;
    process.kind = orthia::g_folderKindProcess;
    process.dianaMode = DIANA_MODE64;
    process.dianaArch = DIANA_ARCH_AARCH64;
    process.pid = 1234;
    process.name = ORTHIA_TCSTR("[1234] n\xC3\xA4me \"x\" <y>");
    process.startUnix = 1792596612;
    process.startKey = "0f1e2d3c-aaaa-bbbb-cccc-0123456789ab:123456";
    orthia::WriteFolderMeta(fileName, process);
    ASSERT_TRUE(orthia::ReadFolderMeta(fileName, meta));
    EXPECT_EQ(orthia::g_folderFormatVersion, meta.formatVersion);
    EXPECT_EQ(orthia::g_folderMinReaderVersion, meta.minReaderVersion);
    EXPECT_EQ(orthia::g_analysisVersion, meta.analysisVersion);
    EXPECT_FALSE(meta.orthiaVersion.empty());
    EXPECT_GT(meta.createdUnix, 0);
    EXPECT_EQ(std::string(orthia::g_folderKindProcess), meta.kind);
    EXPECT_EQ(DIANA_MODE64, meta.dianaMode);
    EXPECT_EQ(DIANA_ARCH_AARCH64, meta.dianaArch);
    EXPECT_EQ(1234ULL, meta.pid);
    EXPECT_EQ(process.name, meta.name);
    EXPECT_EQ(1792596612LL, meta.startUnix);
    EXPECT_EQ(process.startKey, meta.startKey);
    EXPECT_TRUE(orthia::IsFolderReadable(meta));

    // rewritten in place, and a newer writer's folder is not readable
    orthia::FolderMeta file;
    file.kind = orthia::g_folderKindFile;
    file.formatVersion = orthia::g_folderFormatVersion + 1;
    file.minReaderVersion = orthia::g_folderFormatVersion + 1;
    file.format = "elf";
    file.imageBase = 0x400000;
    file.size = 4096;
    file.sha1 = "aabbcc0011223344556677889900aabbccddeeff";
    orthia::WriteFolderMeta(fileName, file);
    ASSERT_TRUE(orthia::ReadFolderMeta(fileName, meta));
    EXPECT_EQ(std::string(orthia::g_folderKindFile), meta.kind);
    EXPECT_EQ(std::string("elf"), meta.format);
    EXPECT_EQ(0x400000ULL, meta.imageBase);
    EXPECT_EQ(4096ULL, meta.size);
    EXPECT_EQ(file.sha1, meta.sha1);
    EXPECT_TRUE(meta.startKey.empty());
    EXPECT_FALSE(orthia::IsFolderReadable(meta));
    EXPECT_FALSE(fs::exists(root / ORTHIA_TCSTR("meta.xml.tmp")));

    // plain XML text: no '\0', which XML tools reject
    {
        std::ifstream stream(fs::path(fileName), std::ios::binary);
        std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        ASSERT_FALSE(text.empty());
        EXPECT_EQ(std::string::npos, text.find('\0'));
        EXPECT_EQ('<', text.front());
    }
    // files written before that end with '\0' and are still read
    {
        std::ofstream stream(fs::path(fileName), std::ios::binary | std::ios::trunc);
        const char old[] = "<root format_version=\"1\" kind=\"file\" sha1=\"aa\"/>";
        stream.write(old, sizeof(old)); // with the terminator
    }
    ASSERT_TRUE(orthia::ReadFolderMeta(fileName, meta));
    EXPECT_EQ(std::string("aa"), meta.sha1);

    // parameters.xml is not a meta.xml
    {
        orthia::CCommonFormatBuilder builder;
        builder.AddMetadata(std::string("format"), std::string("raw"));
        std::vector<char> data;
        builder.Produce(&data);
        std::ofstream(fs::path(fileName), std::ios::binary | std::ios::trunc).write(data.data(), (std::streamsize)data.size());
    }
    EXPECT_FALSE(orthia::ReadFolderMeta(fileName, meta));
}

TEST(Databases, ResolveSelectors)
{
    std::vector<orthia::DatabaseInfo> all = {
        MakeFileDatabase(ORTHIA_TCSTR("aabbcc0011223344556677889900aabbccddeeff")),
        MakeFileDatabase(ORTHIA_TCSTR("aabbcc9911223344556677889900aabbccddeeff")),
        MakeFileDatabase(ORTHIA_TCSTR("1234560011223344556677889900aabbccddeeff")),
        MakeProcDatabase(ORTHIA_TCSTR("[123456] app.exe")),
        MakeProcDatabase(ORTHIA_TCSTR("[42] tool.exe")),
    };
    auto noFiles = [](const orthia::PlatformString_type&) -> orthia::PlatformString_type {
        throw std::runtime_error("unexpected file");
    };
    auto resolve = [&](std::vector<orthia::PlatformString_type> selectors) {
        return orthia::ResolveDatabaseSelectors(all, selectors, noFiles);
    };

    // a unique prefix, case-insensitive
    auto result = resolve({ ORTHIA_TCSTR("AABBCC00") });
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(all[0].folder, result[0].folder);

    // ambiguous prefix, too short prefix, no match
    EXPECT_THROW(resolve({ ORTHIA_TCSTR("aabbcc") }), std::runtime_error);
    EXPECT_THROW(resolve({ ORTHIA_TCSTR("aabb") }), std::runtime_error);
    EXPECT_THROW(resolve({ ORTHIA_TCSTR("ffffff") }), std::runtime_error);

    // a pid
    result = resolve({ ORTHIA_TCSTR("42") });
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(all[4].folder, result[0].folder);

    // a number that is both a pid and a sha1 prefix
    EXPECT_THROW(resolve({ ORTHIA_TCSTR("123456") }), std::runtime_error);

    // duplicates are resolved once; one bad selector fails them all
    result = resolve({ ORTHIA_TCSTR("aabbcc00"), ORTHIA_TCSTR("AABBCC0011") });
    EXPECT_EQ(1u, result.size());
    EXPECT_THROW(resolve({ ORTHIA_TCSTR("aabbcc00"), ORTHIA_TCSTR("ffffff") }), std::runtime_error);

    // anything else is a file name, found by its hash
    result = orthia::ResolveDatabaseSelectors(all, { ORTHIA_TCSTR("some/file.exe") },
        [&](const orthia::PlatformString_type&) { return orthia::PlatformString_type(ORTHIA_TCSTR("AABBCC9911223344556677889900AABBCCDDEEFF")); });
    ASSERT_EQ(1u, result.size());
    EXPECT_EQ(all[1].folder, result[0].folder);
}

TEST(Databases, SelectForCleanup)
{
    const auto now = std::filesystem::file_time_type::clock::now();
    const auto old = now - std::chrono::hours(49);

    auto fresh = MakeFileDatabase(ORTHIA_TCSTR("aa00000000000000000000000000000000000000"));
    auto broken = MakeFileDatabase(ORTHIA_TCSTR("bb00000000000000000000000000000000000000"));
    broken.hasDataDb = false;
    broken.lastWrite = old;
    auto brokenNew = MakeFileDatabase(ORTHIA_TCSTR("cc00000000000000000000000000000000000000"));
    brokenNew.hasDataDb = false;
    auto oldFile = MakeFileDatabase(ORTHIA_TCSTR("dd00000000000000000000000000000000000000"));
    oldFile.lastWrite = old;
    auto pending = MakeFileDatabase(ORTHIA_TCSTR("ee.deleting"));
    pending.pendingDelete = true;

    auto alive = MakeProcDatabase(ORTHIA_TCSTR("[1] alive.exe"));
    auto gone = MakeProcDatabase(ORTHIA_TCSTR("[2] gone.exe"));
    auto reused = MakeProcDatabase(ORTHIA_TCSTR("[3] reused.exe"));
    auto unknown = MakeProcDatabase(ORTHIA_TCSTR("[4] denied.exe"));
    auto expired = MakeProcDatabase(ORTHIA_TCSTR("[5] expired.exe"));
    expired.lastWrite = old;
    auto noPid = MakeProcDatabase(ORTHIA_TCSTR("noname"));
    auto openGone = MakeProcDatabase(ORTHIA_TCSTR("[6] open.exe"));
    auto openBroken = broken;
    openBroken.folder = ORTHIA_TCSTR("db/open-broken");
    // floating code saved: evidence, kept when its process has exited or it is old
    auto evidenceGone = MakeProcDatabase(ORTHIA_TCSTR("[7] evidence.exe"));
    evidenceGone.hasFloatingSnapshots = true;
    auto evidenceOld = MakeProcDatabase(ORTHIA_TCSTR("[8] evidence.exe"));
    evidenceOld.hasFloatingSnapshots = true;
    evidenceOld.lastWrite = old;

    std::vector<orthia::DatabaseInfo> all = { fresh, broken, brokenNew, oldFile, pending,
        alive, gone, reused, unknown, expired, noPid, openGone, openBroken, evidenceGone, evidenceOld };
    std::map<orthia::PlatformString_type, orthia::PlatformString_type> open = {
        { openGone.folder, ORTHIA_TCSTR("open.exe") },
        { openBroken.folder, ORTHIA_TCSTR("x") },
    };
    auto state = [](const orthia::DatabaseInfo& info) {
        switch (info.pid)
        {
        case 1: return orthia::ProcessState::Alive;
        case 3: return orthia::ProcessState::Reused;
        case 4: return orthia::ProcessState::Unknown;
        default: return orthia::ProcessState::Gone;
        }
    };
    std::set<orthia::PlatformString_type> selected;
    for (auto& entry : orthia::SelectDatabasesForCleanup(all, open, now, state))
    {
        selected.insert(entry.info.folder);
    }
    std::set<orthia::PlatformString_type> expected = { broken.folder, pending.folder, gone.folder, reused.folder, expired.folder };
    EXPECT_EQ(expected, selected);
}

static orthia::NameInfo MakeName(int flags, orthia::Address_type address, const orthia::PlatformString_type& name)
{
    orthia::NameInfo info;
    info.flags = flags;
    info.address = address;
    info.name.native = name;
    return info;
}

// "E alpha", "I imp_a", "P zeta": the type and the name identify a row of the test module
static std::vector<orthia::PlatformString_type> DescribeNames(const std::vector<orthia::NameInfo>& names)
{
    std::vector<orthia::PlatformString_type> result;
    for (const auto& info : names)
    {
        orthia::PlatformString_type text;
        if (info.flags & orthia::NameInfo::flags_Export)
            text = ORTHIA_TCSTR("E ");
        else if (info.flags & orthia::NameInfo::flags_Import)
            text = ORTHIA_TCSTR("I ");
        else
            text = ORTHIA_TCSTR("P ");
        result.push_back(text + info.name.native);
    }
    return result;
}

static void FillTestModuleNames(orthia::ModuleNames& names)
{
    const int E = orthia::NameInfo::flags_Export;
    const int I = orthia::NameInfo::flags_Import;
    const int P = orthia::NameInfo::flags_PrivateSymbol;
    // not in any order; alias_alpha is an export alias, P alpha is the PDB record of an export
    names.Add(MakeName(P, 0x10, ORTHIA_TCSTR("zeta")));
    names.Add(MakeName(I, 0x500, ORTHIA_TCSTR("Imp_b")));
    names.Add(MakeName(E, 0x30, ORTHIA_TCSTR("Beta")));
    names.Add(MakeName(E, 0x20, ORTHIA_TCSTR("alpha")));
    names.Add(MakeName(E, 0x20, ORTHIA_TCSTR("alias_alpha")));
    names.Add(MakeName(P, 0x20, ORTHIA_TCSTR("alpha")));
    names.Add(MakeName(I, 0x400, ORTHIA_TCSTR("imp_a")));
    names.Add(MakeName(P, 0x30, ORTHIA_TCSTR("Gamma")));
    names.Add(MakeName(P, 0x5, ORTHIA_TCSTR("delta")));
    names.Finalize();
}

static std::vector<orthia::PlatformString_type> QueryAllNames(const orthia::ModuleNames& names, orthia::NameSelectionKey key)
{
    std::vector<orthia::NameInfo> page;
    key.offset = 0;
    names.QueryPage(key, 1000, page);
    return DescribeNames(page);
}

TEST(ModuleNames, SortOrders)
{
    orthia::ModuleNames names;
    FillTestModuleNames(names);
    orthia::NameSelectionKey key;

    // exports, imports, private symbols; each group by address
    std::vector<orthia::PlatformString_type> expected = {
        ORTHIA_TCSTR("E alpha"), ORTHIA_TCSTR("E alias_alpha"), ORTHIA_TCSTR("E Beta"),
        ORTHIA_TCSTR("I imp_a"), ORTHIA_TCSTR("I Imp_b"),
        ORTHIA_TCSTR("P delta"), ORTHIA_TCSTR("P zeta"), ORTHIA_TCSTR("P alpha"), ORTHIA_TCSTR("P Gamma") };
    EXPECT_EQ(expected, QueryAllNames(names, key));
    EXPECT_EQ(9, names.QueryCount(key));

    // case-insensitive, then by address, then by type
    key.sortOrder = orthia::NameSortOrder::Name;
    expected = {
        ORTHIA_TCSTR("E alias_alpha"), ORTHIA_TCSTR("E alpha"), ORTHIA_TCSTR("P alpha"), ORTHIA_TCSTR("E Beta"),
        ORTHIA_TCSTR("P delta"), ORTHIA_TCSTR("P Gamma"), ORTHIA_TCSTR("I imp_a"), ORTHIA_TCSTR("I Imp_b"),
        ORTHIA_TCSTR("P zeta") };
    EXPECT_EQ(expected, QueryAllNames(names, key));

    // by address, then by type, then by name
    key.sortOrder = orthia::NameSortOrder::Address;
    expected = {
        ORTHIA_TCSTR("P delta"), ORTHIA_TCSTR("P zeta"),
        ORTHIA_TCSTR("E alias_alpha"), ORTHIA_TCSTR("E alpha"), ORTHIA_TCSTR("P alpha"),
        ORTHIA_TCSTR("E Beta"), ORTHIA_TCSTR("P Gamma"),
        ORTHIA_TCSTR("I imp_a"), ORTHIA_TCSTR("I Imp_b") };
    EXPECT_EQ(expected, QueryAllNames(names, key));
}

TEST(ModuleNames, Filters)
{
    orthia::ModuleNames names;
    FillTestModuleNames(names);

    orthia::NameSelectionKey key;
    key.excludeImports = true;
    std::vector<orthia::PlatformString_type> expected = {
        ORTHIA_TCSTR("E alpha"), ORTHIA_TCSTR("E alias_alpha"), ORTHIA_TCSTR("E Beta"),
        ORTHIA_TCSTR("P delta"), ORTHIA_TCSTR("P zeta"), ORTHIA_TCSTR("P alpha"), ORTHIA_TCSTR("P Gamma") };
    EXPECT_EQ(expected, QueryAllNames(names, key));
    EXPECT_EQ(7, names.QueryCount(key));

    key = orthia::NameSelectionKey();
    key.privateSymbolsOnly = true;
    key.sortOrder = orthia::NameSortOrder::Name;
    expected = { ORTHIA_TCSTR("P alpha"), ORTHIA_TCSTR("P delta"), ORTHIA_TCSTR("P Gamma"), ORTHIA_TCSTR("P zeta") };
    EXPECT_EQ(expected, QueryAllNames(names, key));
    EXPECT_EQ(4, names.QueryCount(key));
}

TEST(ModuleNames, Paging)
{
    orthia::ModuleNames names;
    FillTestModuleNames(names);

    // like the names panel: the next page starts where the cached rows end
    for (auto sortOrder : { orthia::NameSortOrder::Type, orthia::NameSortOrder::Name, orthia::NameSortOrder::Address })
    {
        for (int filter = 0; filter < 3; ++filter)
        {
            orthia::NameSelectionKey key;
            key.sortOrder = sortOrder;
            key.excludeImports = filter == 1;
            key.privateSymbolsOnly = filter == 2;
            auto expected = QueryAllNames(names, key);
            for (int pageSize : { 1, 2, 3, 7 })
            {
                std::vector<orthia::NameInfo> all, page;
                key.offset = 0;
                for (;;)
                {
                    names.QueryPage(key, pageSize, page);
                    if (page.empty())
                        break;
                    EXPECT_LE((int)page.size(), pageSize);
                    all.insert(all.end(), page.begin(), page.end());
                    key.offset += (int)page.size();
                }
                EXPECT_EQ(expected, DescribeNames(all)) << "order " << (int)sortOrder << ", filter " << filter << ", page " << pageSize;
            }
        }
    }

    // no stale rows are left in the output
    std::vector<orthia::NameInfo> page = { MakeName(0, 1, ORTHIA_TCSTR("stale")) };
    orthia::NameSelectionKey key;
    names.QueryPage(key, 0, page);
    EXPECT_TRUE(page.empty());
    page = { MakeName(0, 1, ORTHIA_TCSTR("stale")) };
    key.offset = 9;
    names.QueryPage(key, 10, page);
    EXPECT_TRUE(page.empty());
}

TEST(ModuleNames, Storage)
{
    orthia::ModuleNamesStorage storage;
    std::map<orthia::Address_type, int> builds;
    auto query = [&](orthia::Address_type moduleAddress) {
        return storage.Query(moduleAddress, [&](orthia::ModuleNames& names) {
            ++builds[moduleAddress];
            names.Add(MakeName(orthia::NameInfo::flags_Export, moduleAddress, ORTHIA_TCSTR("name")));
        });
    };

    auto names = query(1);
    EXPECT_EQ(names, query(1));
    EXPECT_EQ(1, builds[1]);

    // the least recently used module is dropped
    for (orthia::Address_type moduleAddress = 2; moduleAddress <= 5; ++moduleAddress)
        query(moduleAddress);
    query(1);
    EXPECT_EQ(2, builds[1]);
    query(5);
    EXPECT_EQ(1, builds[5]);

    // only the invalidated module is read again
    storage.Invalidate(5);
    query(5);
    query(4);
    EXPECT_EQ(2, builds[5]);
    EXPECT_EQ(1, builds[4]);

    storage.Clear();
    query(4);
    EXPECT_EQ(2, builds[4]);
}

#ifndef _WIN32
TEST(Config, ExpandHomeFolder)
{
    EXPECT_EQ("/home/u/sym", orthia::ExpandHomeFolder("~/sym", "/home/u"));
    EXPECT_EQ("/home/u/sym", orthia::ExpandHomeFolder("~/sym", "/home/u/"));
    EXPECT_EQ("/home/u", orthia::ExpandHomeFolder("~", "/home/u"));
    EXPECT_EQ("/sym", orthia::ExpandHomeFolder("~/sym", "/"));
    EXPECT_EQ("~other/sym", orthia::ExpandHomeFolder("~other/sym", "/home/u"));
    EXPECT_EQ("/opt/~/sym", orthia::ExpandHomeFolder("/opt/~/sym", "/home/u"));
    EXPECT_EQ("~/sym", orthia::ExpandHomeFolder("~/sym", ""));
}

TEST(Privileges, ParseIdText)
{
    unsigned int id = 7;
    EXPECT_TRUE(orthia::ParseIdText("1000", &id));
    EXPECT_EQ(1000u, id);
    EXPECT_TRUE(orthia::ParseIdText("0", &id));
    EXPECT_EQ(0u, id);
    EXPECT_TRUE(orthia::ParseIdText("4294967294", &id));
    EXPECT_EQ(4294967294u, id);
    // (uid_t)-1 means "unchanged" to setresuid
    EXPECT_FALSE(orthia::ParseIdText("4294967295", &id));
    EXPECT_FALSE(orthia::ParseIdText("99999999999", &id));
    EXPECT_FALSE(orthia::ParseIdText("", &id));
    EXPECT_FALSE(orthia::ParseIdText(nullptr, &id));
    EXPECT_FALSE(orthia::ParseIdText("-1", &id));
    EXPECT_FALSE(orthia::ParseIdText(" 1000", &id));
    EXPECT_FALSE(orthia::ParseIdText("1000x", &id));
    EXPECT_FALSE(orthia::ParseIdText("0x10", &id));
}
#endif

TEST(ElfDebugFile, HostLookup)
{
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path root = fs::temp_directory_path() / ORTHIA_TCSTR("orthia_test_elf_debug_file");
    fs::remove_all(root, error);
    const fs::path symbols = root / ORTHIA_TCSTR("symbols");
    const fs::path moduleDir = root / ORTHIA_TCSTR("lib");
    fs::create_directories(symbols / ORTHIA_TCSTR(".build-id") / ORTHIA_TCSTR("ab"));
    fs::create_directories(moduleDir / ORTHIA_TCSTR(".debug"));
    auto touch = [](const fs::path& path) { std::ofstream file(path); };
    const fs::path module = moduleDir / ORTHIA_TCSTR("libx.so");
    touch(module);
    const fs::path byBuildId = symbols / ORTHIA_TCSTR(".build-id") / ORTHIA_TCSTR("ab") / ORTHIA_TCSTR("cdef.debug");
    touch(byBuildId);
    const fs::path byLink = moduleDir / ORTHIA_TCSTR(".debug") / ORTHIA_TCSTR("libx.so.debug");
    touch(byLink);
    const std::vector<orthia::PlatformString_type> folders = { symbols.native() };

    // the build-id comes first
    EXPECT_EQ(byBuildId.native(), orthia::FindElfDebugFile(module.native(), ORTHIA_TCSTR("abcdef"), ORTHIA_TCSTR("libx.so.debug"), folders));
    // then the debug link; a directory part in it is ignored
    EXPECT_EQ(byLink.native(), orthia::FindElfDebugFile(module.native(), ORTHIA_TCSTR("ab0000"), ORTHIA_TCSTR("libx.so.debug"), folders));
    EXPECT_EQ(byLink.native(), orthia::FindElfDebugFile(module.native(), orthia::PlatformString_type(), ORTHIA_TCSTR("../x/libx.so.debug"), folders));
    // a debug link or a build-id alone, with nothing installed, is not reported
    EXPECT_TRUE(orthia::FindElfDebugFile(module.native(), ORTHIA_TCSTR("ab0000"), ORTHIA_TCSTR("other.debug"), folders).empty());
    EXPECT_TRUE(orthia::FindElfDebugFile(module.native(), orthia::PlatformString_type(), orthia::PlatformString_type(), folders).empty());
    // a debug link naming the module itself
    EXPECT_TRUE(orthia::FindElfDebugFile(module.native(), orthia::PlatformString_type(), ORTHIA_TCSTR("libx.so"), folders).empty());

    fs::remove_all(root, error);
}

TEST(ImageFormats, Registry)
{
    std::set<std::string> ids;
    int typeFlags = 0;
    for (auto* format : orthia::GetImageFormats())
    {
        ASSERT_NE(nullptr, format);
        EXPECT_TRUE(ids.insert(format->GetId()).second) << format->GetId();
        EXPECT_EQ(format, orthia::FindFormatById(format->GetId()));
        EXPECT_NE(std::string(), format->GetDisplayName());
        // one bit per format, apart from the status bits that share the field
        const int flag = format->GetBuiltInFlag();
        EXPECT_NE(0, flag);
        EXPECT_EQ(0, flag & (flag - 1)) << format->GetId();
        EXPECT_EQ(0, flag & typeFlags) << format->GetId();
        EXPECT_EQ(0, flag & orthia::ModuleInfo::builtInFlags_unresolved) << format->GetId();
        EXPECT_EQ(0, flag & orthia::ModuleInfo::builtInFlags_managed) << format->GetId();
        typeFlags |= flag;
    }
    EXPECT_EQ((std::set<std::string>{ "pe", "elf", "raw" }), ids);
    EXPECT_STREQ("PE", orthia::FindFormatById("pe")->GetDisplayName());
    EXPECT_STREQ("ELF", orthia::FindFormatById("elf")->GetDisplayName());
    // the ids are stored, so they stay lowercase and exact
    EXPECT_EQ(nullptr, orthia::FindFormatById("PE"));
    EXPECT_EQ(nullptr, orthia::FindFormatById(""));
    // raw code is chosen by the user, never detected
    EXPECT_TRUE(orthia::FindFormatById("raw")->NeedsUserParameters());
    EXPECT_FALSE(orthia::FindFormatById("raw")->Detect("MZ", 2));
}

TEST(ImageFormats, ModuleStatusText)
{
    orthia::ModuleInfo mod;
    EXPECT_EQ(orthia::PlatformString_type(), orthia::ModuleStatusText(mod));
    mod.flags = orthia::ModuleInfo::flags_analyzeDone | orthia::ModuleInfo::flags_symbolsLoaded;
    EXPECT_EQ(ORTHIA_TCSTR("analyzed, symbols"), orthia::ModuleStatusText(mod));
    // .NET first: the modules window column cuts the end
    mod.builtInFlags = orthia::ModuleInfo::builtInFlags_moduleTypePe | orthia::ModuleInfo::builtInFlags_managed;
    mod.imageSourceKind = (int)orthia::ImageSourceKind::LinkedFile;
    EXPECT_EQ(ORTHIA_TCSTR(".NET, linked, analyzed, symbols"), orthia::ModuleStatusText(mod));
    mod.builtInFlags |= orthia::ModuleInfo::builtInFlags_unresolved;
    mod.flags = 0;
    EXPECT_EQ(ORTHIA_TCSTR(".NET, unresolved"), orthia::ModuleStatusText(mod));
}

TEST(ImageFormats, Detect)
{
    auto detect = [](const std::string& header) {
        auto* format = orthia::FindFormatByData(header.data(), header.size());
        return format ? std::string(format->GetId()) : std::string("none");
    };
    EXPECT_EQ("pe", detect(std::string("MZ\x90\0", 4)));
    EXPECT_EQ("elf", detect(std::string("\x7f" "ELF\x02\x01\x01", 7)));
    EXPECT_EQ("none", detect("just some text"));
    EXPECT_EQ("none", detect(std::string("\x55\x48\x89\xe5", 4)));
    // too short to tell
    EXPECT_EQ("none", detect("M"));
    EXPECT_EQ("none", detect(std::string("\x7f" "EL", 3)));
    EXPECT_EQ("none", detect(""));
}

TEST(ImageFormats, MapErrorMessage)
{
    // recognized, but no valid headers behind the magic
    std::vector<char> data(0x200, 0);
    data[0] = 'M';
    data[1] = 'Z';
    try
    {
        orthia::FindFormatById("pe")->MapFile(data, orthia::MapFileParameters());
        FAIL() << "mapped a PE file without NT headers";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_EQ(0u, std::string(e.what()).find("Invalid PE image: ")) << e.what();
    }

    std::vector<char> elf(0x40, 0);
    elf[0] = 0x7f;
    elf[1] = 'E';
    elf[2] = 'L';
    elf[3] = 'F';
    try
    {
        orthia::FindFormatById("elf")->MapFile(elf, orthia::MapFileParameters());
        FAIL() << "mapped an ELF file with an empty header";
    }
    catch (const std::runtime_error& e)
    {
        EXPECT_EQ(0u, std::string(e.what()).find("Invalid ELF image: ")) << e.what();
    }
}

namespace
{
    // A process made of images that the formats mapped: what the OS layer of a live process reports,
    // for machines this host can't run. The modules are opened as both OS layers open them
    class CFakeProcess : public oui::IProcess
    {
    public:
        struct Image
        {
            const orthia::IImageFormat* format = nullptr;
            orthia::Address_type base = 0;
            std::vector<char> data;
        };
        // memory no module owns: heap, JIT, injected code
        struct Region
        {
            orthia::Address_type base = 0;
            std::vector<char> data;
            int protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write |
                          oui::MemoryRegionInfo::protect_exec;
            // a part of the allocation that starts there (0: its own allocation)
            orthia::Address_type allocationBase = 0;
        };
        int dianaMode = DIANA_MODE64;
        int dianaArch = DIANA_ARCH_X86;
        std::vector<Image> images;
        std::vector<Region> regions;

        int QueryMemoryRegion(unsigned long long address, oui::MemoryRegionInfo& info) override
        {
            info = oui::MemoryRegionInfo();
            auto fill = [&](orthia::Address_type base, size_t size, int protect, int type) {
                info.allocationBase = info.regionBase = base;
                info.allocationSize = info.regionSize = size;
                info.protect = protect;
                info.committed = true;
                info.type = type;
            };
            for (const auto& region : regions)
            {
                if (address >= region.base && address - region.base < region.data.size())
                {
                    fill(region.base, region.data.size(), region.protect, oui::MemoryRegionInfo::type_private);
                    if (region.allocationBase)
                    {
                        // the allocation: every part with the same allocation base
                        info.allocationBase = region.allocationBase;
                        orthia::Address_type end = region.allocationBase;
                        for (const auto& part : regions)
                        {
                            if (part.allocationBase == region.allocationBase)
                            {
                                end = std::max<orthia::Address_type>(end, part.base + part.data.size());
                            }
                        }
                        info.allocationSize = end - region.allocationBase;
                    }
                    return 0;
                }
            }
            for (const auto& image : images)
            {
                if (address >= image.base && address - image.base < image.data.size())
                {
                    fill(image.base, image.data.size(), oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec,
                         oui::MemoryRegionInfo::type_image);
                    return 0;
                }
            }
            return 1;
        }

        std::tuple<int, unsigned long long> GetSizeInBytes() const override
        {
            return std::make_tuple(0, dianaMode == DIANA_MODE32 ? 0xFFFFFFFFull : ~0ull);
        }
        int ReadExact(std::shared_ptr<oui::BaseOperation>, unsigned long long offset, size_t size, std::vector<char>& data) override
        {
            data.resize(size);
            return ReadExactEx2(offset, data.data(), size);
        }
        oui::String GetFullFileName() const override
        {
            oui::String name;
            name.native = OUI_TCSTR("[1] fake");
            return name;
        }
        oui::String GetFullFileNameForUI() const override
        {
            return GetFullFileName();
        }
        int ReadExactEx2(unsigned long long offset, void* pBuffer, size_t size) override
        {
            memset(pBuffer, 0, size);
            bool found = false;
            auto copy = [&](orthia::Address_type base, const std::vector<char>& data) {
                const unsigned long long dataEnd = base + data.size();
                const unsigned long long start = std::max<unsigned long long>(offset, base);
                const unsigned long long end = std::min<unsigned long long>(offset + size, dataEnd);
                if (start < end)
                {
                    memcpy((char*)pBuffer + (start - offset), data.data() + (start - base), (size_t)(end - start));
                    found = true;
                }
            };
            for (const auto& image : images)
            {
                copy(image.base, image.data);
            }
            for (const auto& region : regions)
            {
                copy(region.base, region.data);
            }
            return found ? 0 : 1;
        }
        orthia::WorkAddressData ReadExactEx(unsigned long long offset, size_t size) override
        {
            std::vector<char> buffer(size);
            if (ReadExactEx2(offset, buffer.data(), size))
            {
                return orthia::WorkAddressData();
            }
            auto* pBufferStart = buffer.data();
            return orthia::WorkAddressData(pBufferStart, size, nullptr, orthia::WorkAddressData::flags_FullValid,
                [buffer = std::move(buffer)](orthia::WorkAddressData*) mutable {});
        }
        int QueryModules(std::vector<orthia::ModuleInfo>& modules, int& processModuleOffset,
            std::function<void(const orthia::ModuleInfo& info, oui::ModuleDisasmContext&)> contextCallback) override
        {
            processModuleOffset = 0;
            modules.clear();
            orthia::ProcessReaderAdapter reader(this);
            for (const auto& image : images)
            {
                orthia::ModuleInfo info;
                info.name = OUI_TCSTR("module");
                info.address = image.base;
                info.size = image.data.size();
                info.lastValidAddress = image.base + image.data.size() - 1;
                if (auto moduleImage = image.format->OpenModuleImage(reader, image.base, image.data.size()))
                {
                    const auto range = moduleImage->GetRange();
                    info.dianaMode = range.dianaMode;
                    info.dianaArch = range.dianaArch;
                    info.entryPoint = range.entryPoint ? range.entryPoint : info.address;
                    info.builtInFlags |= image.format->GetBuiltInFlag() | moduleImage->GetExtraBuiltInFlags();
                    oui::ModuleDisasmContext context;
                    context.image = moduleImage.get();
                    contextCallback(info, context);
                }
                modules.push_back(info);
            }
            return 0;
        }
        std::tuple<int, std::shared_ptr<oui::IThreadEnumerator>> CreateThreadEnumerator() override
        {
            return std::make_tuple(1, std::shared_ptr<oui::IThreadEnumerator>());
        }
        int GetDianaMode() const override
        {
            return dianaMode;
        }
        int GetDianaArch() const override
        {
            return dianaArch;
        }
        oui::ProcessStartInfo GetStartInfo() const override
        {
            return oui::ProcessStartInfo();
        }
        bool IsRunning() const override
        {
            return false;
        }
    };

    // data/ of the source tree, above the folder of the binary (bin/Release/amd64, cmake-release/src/...)
    std::filesystem::path FindTestData(const std::filesystem::path& relative)
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::path(orthia::GetCurrentProcessDir()).parent_path();
        for (int i = 0; i < 8; ++i)
        {
            std::error_code error;
            const fs::path candidate = dir / ORTHIA_TCSTR("data") / relative;
            if (fs::exists(candidate, error))
            {
                return candidate;
            }
            if (dir == dir.parent_path())
            {
                break;
            }
            dir = dir.parent_path();
        }
        return fs::path();
    }
}

// An ARM process, as an ARM host reports it: its modules get their architecture from their headers,
// the rest of the address space (heap, JIT code) is the process's own
TEST(ProcessArch, ModulesAndProcessRange)
{
    struct Case
    {
        const char* format;
        const ORTHIA_TCHAR* file;
        int processMode;
        int processArch;
        int moduleMode;
        int moduleArch;
    };
    const Case cases[] = {
        { "elf", ORTHIA_TCSTR("elf/libstartup_arm64.so"), DIANA_MODE64, DIANA_ARCH_AARCH64, DIANA_MODE64, DIANA_ARCH_AARCH64 },
        { "elf", ORTHIA_TCSTR("elf/libstartup_arm32.so"), DIANA_MODE32, DIANA_ARCH_ARM32, DIANA_MODE32, DIANA_ARCH_ARM32 },
        { "elf", ORTHIA_TCSTR("elf/libstartup_thumb.so"), DIANA_MODE32, DIANA_ARCH_ARM32, DIANA_MODE32, DIANA_ARCH_THUMB },
        { "elf", ORTHIA_TCSTR("elf/libstartup_x64.so"), DIANA_MODE64, DIANA_ARCH_X86, DIANA_MODE64, DIANA_ARCH_X86 },
        { "pe", ORTHIA_TCSTR("pe/tls_aarch64.dll"), DIANA_MODE64, DIANA_ARCH_AARCH64, DIANA_MODE64, DIANA_ARCH_AARCH64 },
        { "pe", ORTHIA_TCSTR("pe/tls_armnt.dll"), DIANA_MODE32, DIANA_ARCH_THUMB, DIANA_MODE32, DIANA_ARCH_THUMB },
        // an x64 process emulated on ARM64 Windows loads ARM64X system DLLs: each module keeps its own
        { "pe", ORTHIA_TCSTR("pe/tls_aarch64.dll"), DIANA_MODE64, DIANA_ARCH_X86, DIANA_MODE64, DIANA_ARCH_AARCH64 },
    };
    for (const auto& item : cases)
    {
        const auto path = FindTestData(item.file);
        if (path.empty())
        {
            GTEST_SKIP() << "data/ not found above " << orthia::PlatformStringToUtf8(orthia::GetCurrentProcessDir());
        }
        SCOPED_TRACE(path.string());
        std::vector<char> data;
        {
            std::ifstream file(path, std::ios::binary);
            data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
        const auto* format = orthia::FindFormatById(item.format);
        ASSERT_NE(nullptr, format);
        orthia::MapFileParameters params;
        params.imageBase = 0x10000000;
        const auto mapped = format->MapFile(data, params);

        auto process = std::make_shared<CFakeProcess>();
        process->dianaMode = item.processMode;
        process->dianaArch = item.processArch;
        CFakeProcess::Image image;
        image.format = format;
        image.base = mapped->GetImageBase();
        image.data = mapped->GetMappedFile();
        process->images.push_back(image);

        orthia::CProcessWorkplaceItem processItem(process, process->GetFullFileNameForUI(),
            process->GetDianaMode(), process->GetDianaArch(), nullptr);
        processItem.ReloadModules();
        EXPECT_EQ(item.processMode, processItem.GetDianaMode());
        EXPECT_EQ(item.processArch, processItem.GetDianaArch());

        std::vector<orthia::ModuleInfo> modules;
        processItem.GetModules(modules);
        ASSERT_EQ(1u, modules.size());
        EXPECT_EQ(item.moduleMode, modules[0].dianaMode);
        EXPECT_EQ(item.moduleArch, modules[0].dianaArch);

        const auto inModule = processItem.GetRangeInfo(image.base + 0x10);
        EXPECT_EQ(image.base, inModule.address);
        EXPECT_EQ(item.moduleArch, inModule.dianaArch);

        // below every module: the address space of the process
        const auto outside = processItem.GetRangeInfo(0x1000);
        EXPECT_EQ(0u, outside.address);
        EXPECT_EQ(item.processMode, outside.dianaMode);
        EXPECT_EQ(item.processArch, outside.dianaArch);
    }
}

namespace
{
    // e8/e9 rel32 at offset of code placed at base
    void PutRel32(std::vector<char>& code, size_t offset, unsigned char opcode, orthia::Address_type base, orthia::Address_type target)
    {
        code[offset] = (char)opcode;
        const int rel = (int)(long long)(target - (base + offset + 5));
        memcpy(&code[offset + 1], &rel, sizeof(rel));
    }

    // A process with one module (raw code) that calls into memory no module owns
    struct FloatingFixture
    {
        static constexpr orthia::Address_type moduleBase = 0x10000000;
        static constexpr orthia::Address_type regionA = 0x10100000;     // RWX, called twice
        static constexpr orthia::Address_type otherModule = 0x10200000;
        static constexpr orthia::Address_type regionData = 0x10300000;  // RW-
        static constexpr orthia::Address_type regionD = 0x10400000;     // RWX, A jumps there

        std::filesystem::path root;
        std::shared_ptr<CFakeProcess> process = std::make_shared<CFakeProcess>();
        std::shared_ptr<orthia::CModuleManager> moduleManager = std::make_shared<orthia::CModuleManager>();

        FloatingFixture()
        {
            namespace fs = std::filesystem;
            root = fs::temp_directory_path() / ORTHIA_TCSTR("orthia_test_floating");
            std::error_code ec;
            fs::remove_all(root, ec);
            fs::create_directories(root);
            moduleManager->Reinit((root / ORTHIA_TCSTR("db.sqlite")).native(), true);

            const auto* raw = orthia::FindFormatById("raw");
            CFakeProcess::Image module;
            module.format = raw;
            module.base = moduleBase;
            module.data.assign(0x1000, (char)0xCC);
            PutRel32(module.data, 0, 0xE8, moduleBase, regionA);
            PutRel32(module.data, 5, 0xE8, moduleBase, regionA + 0x20);
            PutRel32(module.data, 10, 0xE8, moduleBase, otherModule);
            PutRel32(module.data, 15, 0xE8, moduleBase, regionData);
            module.data[20] = (char)0xC3;
            process->images.push_back(module);

            CFakeProcess::Image other;
            other.format = raw;
            other.base = otherModule;
            other.data.assign(0x1000, (char)0xC3);
            process->images.push_back(other);

            CFakeProcess::Region a;
            a.base = regionA;
            a.data.assign(0x1000, (char)0xCC);
            PutRel32(a.data, 0, 0xE9, regionA, regionD);
            a.data[0x20] = (char)0xC3;
            process->regions.push_back(a);

            CFakeProcess::Region data;
            data.base = regionData;
            data.data.assign(0x1000, 0);
            data.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write;
            process->regions.push_back(data);

            CFakeProcess::Region d;
            d.base = regionD;
            d.data.assign(0x1000, (char)0xC3);
            process->regions.push_back(d);
        }
        ~FloatingFixture()
        {
            moduleManager.reset();
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
        }
        std::shared_ptr<orthia::CProcessWorkplaceItem> OpenItem()
        {
            auto item = std::make_shared<orthia::CProcessWorkplaceItem>(process, process->GetFullFileNameForUI(),
                process->GetDianaMode(), process->GetDianaArch(), nullptr);
            item->ReloadModules();
            item->Init(moduleManager, nullptr, nullptr);
            return item;
        }
        // the module analyzed, as .analyze leaves it
        void AnalyzeModule(orthia::CProcessWorkplaceItem& item)
        {
            orthia::ProcessReaderAdapter reader(process.get());
            moduleManager->ReloadRange(moduleBase, 0x1000, &reader, DIANA_MODE64, DIANA_ARCH_X86, 0);
            item.UpdateModuleFlags(moduleBase, orthia::ModuleInfo::flags_analyzeDone, 0);
        }
        static orthia::ModuleInfo FindModule(const orthia::IWorkPlaceItem& item, orthia::Address_type address)
        {
            std::vector<orthia::ModuleInfo> modules;
            item.GetModules(modules);
            for (const auto& mod : modules)
            {
                if (mod.address == address)
                    return mod;
            }
            return orthia::ModuleInfo();
        }
        // what the floating task does until nothing waits
        static void AnalyzePending(orthia::IWorkPlaceItem& item)
        {
            for (int pass = 0; pass < 10; ++pass)
            {
                auto pending = orthia::QueryPendingFloatingModules(item);
                if (pending.empty())
                    return;
                for (const auto& mod : pending)
                {
                    orthia::AnalyzeFloatingModule(item, mod);
                    orthia::DiscoverFloatingCode(item, FindModule(item, mod.address));
                }
            }
            FAIL() << "the floating modules don't stop coming";
        }
    };
}

// TODO: the same tests for AArch64 and AArch32 (A32, T32 with bit 0 of the entry) floating code

// x86 call targets outside the module are saved as absolute addresses, also below the module
TEST(FloatingCode, ExternalTargetsAreAbsolute)
{
    struct Case
    {
        int mode;
        orthia::Address_type base;
        orthia::Address_type target;
    };
    const Case cases[] = {
        { DIANA_MODE64, 0x10000000, 0x10100000 },
        { DIANA_MODE64, 0x10000000, 0x00400000 },
        { DIANA_MODE32, 0x10000000, 0x10100000 },
        { DIANA_MODE32, 0x10000000, 0x00400000 },
    };
    for (const auto& item : cases)
    {
        SCOPED_TRACE(std::to_string(item.mode) + " " + std::to_string(item.target));
        FloatingFixture fixture;
        fixture.process->images.clear();
        fixture.process->dianaMode = item.mode;
        CFakeProcess::Image module;
        module.format = orthia::FindFormatById("raw");
        module.base = item.base;
        module.data.assign(0x100, (char)0xCC);
        PutRel32(module.data, 0, 0xE8, item.base, item.target);
        PutRel32(module.data, 5, 0xE8, item.base, item.base + 0x80);
        module.data[10] = (char)0xC3;
        module.data[0x80] = (char)0xC3;
        fixture.process->images.push_back(module);

        orthia::ProcessReaderAdapter reader(fixture.process.get());
        fixture.moduleManager->ReloadRange(item.base, 0x100, &reader, item.mode, DIANA_ARCH_X86, 0);
        std::vector<std::pair<orthia::Address_type, orthia::Address_type>> refs;
        fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->QueryAllModuleReferences(item.base, 0x100, refs);
        bool external = false;
        bool internal = false;
        for (const auto& ref : refs)
        {
            external |= ref.first == item.base && ref.second == item.target;
            internal |= ref.first == item.base + 5 && ref.second == item.base + 0x80;
        }
        EXPECT_TRUE(external);
        EXPECT_TRUE(internal);
    }
}

TEST(FloatingCode, RegionTextRoundTrip)
{
    orthia::FloatingRegionInfo info;
    info.allocationBase = 0x7ff600010000ULL;
    info.allocationSize = 0x3000;
    info.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_exec;
    info.type = oui::MemoryRegionInfo::type_mapped;
    info.mappedName = "/tmp/a b.bin";
    info.header = "MZ";
    info.bytesHash = 0xfedcba9876543210ULL;
    info.hashedSize = 0x3000;
    info.codeStart = 0x7ff600010010ULL;
    info.codeLast = 0x7ff600010040ULL;
    info.AddReference(0x7ff600010040ULL, 0x401000);
    info.AddReference(0x7ff600010010ULL, 0x401020);
    info.AddReference(0x7ff600010010ULL, 0x401010);
    // an import slot, an export entry; the name keeps the separators of the text
    orthia::PointerSlotInfo import;
    import.slot = 0x403008;
    import.kind = orthia::PointerSlotInfo::kind_import;
    import.name = "KERNEL32.dll!Create,File;W:x=%";
    info.AddSlot(0x7ff600010040ULL, import);
    orthia::PointerSlotInfo exported;
    exported.slot = 0x402000;
    exported.kind = orthia::PointerSlotInfo::kind_export;
    exported.name = "NtClose";
    info.AddSlot(0x7ff600010040ULL, exported);
    EXPECT_FALSE(info.AddSlot(0x7ff600010040ULL, exported));

    orthia::FloatingRegionInfo parsed;
    ASSERT_TRUE(orthia::ParseFloatingRegion(orthia::FormatFloatingRegion(info), parsed));
    EXPECT_EQ(info.allocationBase, parsed.allocationBase);
    EXPECT_EQ(info.allocationSize, parsed.allocationSize);
    EXPECT_EQ(info.protect, parsed.protect);
    EXPECT_EQ(info.type, parsed.type);
    EXPECT_EQ(info.mappedName, parsed.mappedName);
    EXPECT_EQ(info.header, parsed.header);
    EXPECT_EQ(info.bytesHash, parsed.bytesHash);
    EXPECT_EQ(info.hashedSize, parsed.hashedSize);
    EXPECT_EQ(info.codeStart, parsed.codeStart);
    EXPECT_EQ(info.codeLast, parsed.codeLast);
    ASSERT_EQ(2u, parsed.entries.size());
    EXPECT_EQ(0x7ff600010010ULL, parsed.entries[0].address);
    EXPECT_EQ((std::vector<orthia::Address_type>{ 0x401010, 0x401020 }), parsed.entries[0].referrers);
    EXPECT_EQ(0x7ff600010040ULL, parsed.entries[1].address);
    EXPECT_TRUE(parsed.entries[0].slots.empty());
    ASSERT_EQ(2u, parsed.entries[1].slots.size());
    EXPECT_EQ(0x402000u, parsed.entries[1].slots[0].slot);
    EXPECT_EQ(orthia::PointerSlotInfo::kind_export, parsed.entries[1].slots[0].kind);
    EXPECT_EQ("NtClose", parsed.entries[1].slots[0].name);
    EXPECT_EQ(0x403008u, parsed.entries[1].slots[1].slot);
    EXPECT_EQ(orthia::PointerSlotInfo::kind_import, parsed.entries[1].slots[1].kind);
    EXPECT_EQ(import.name, parsed.entries[1].slots[1].name);

    EXPECT_FALSE(orthia::ParseFloatingRegion("<x/>", parsed));
    EXPECT_EQ(ORTHIA_TCSTR("fl_7ff600010000"), orthia::MakeFloatingModuleName(info.allocationBase));
    EXPECT_EQ(ORTHIA_TCSTR("R-X"), orthia::FormatRegionProtect(info.protect));
}

TEST(FloatingCode, DiscoveryAndAnalysis)
{
    FloatingFixture fixture;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);

    const auto source = FloatingFixture::FindModule(*item, FloatingFixture::moduleBase);
    ASSERT_EQ(FloatingFixture::moduleBase, source.address);
    EXPECT_TRUE(orthia::DiscoverFloatingCode(*item, source));

    // one module for the two calls into A; the other module and the data are not floating code
    std::vector<orthia::ModuleInfo> modules;
    item->GetModules(modules);
    ASSERT_EQ(3u, modules.size());
    const auto a = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
    ASSERT_TRUE(a.IsFloating());
    ASSERT_TRUE(a.floating);
    EXPECT_EQ(ORTHIA_TCSTR("fl_10100000"), a.name);
    EXPECT_EQ(0x1000u, a.size);
    EXPECT_EQ(oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write | oui::MemoryRegionInfo::protect_exec,
        a.floating->protect);
    ASSERT_EQ(2u, a.floating->entries.size());
    EXPECT_EQ(FloatingFixture::regionA, a.floating->entries[0].address);
    EXPECT_EQ((std::vector<orthia::Address_type>{ FloatingFixture::moduleBase }), a.floating->entries[0].referrers);
    EXPECT_EQ(FloatingFixture::regionA + 0x20, a.floating->entries[1].address);
    EXPECT_EQ((std::vector<orthia::Address_type>{ FloatingFixture::moduleBase + 5 }), a.floating->entries[1].referrers);
    EXPECT_FALSE(a.flags & orthia::ModuleInfo::flags_analyzeDone);

    // the names: x fl_10100000!*, and the address of fl_10100000!$entry_0002
    std::vector<orthia::NameInfo> names;
    item->QueryNames(a.address, orthia::NameSelectionKey(), 10, names);
    ASSERT_EQ(2u, names.size());
    EXPECT_EQ(ORTHIA_TCSTR("$entry_0001"), names[0].name.native);
    EXPECT_EQ(FloatingFixture::regionA + 0x20, names[1].address);
    EXPECT_EQ(FloatingFixture::regionA + 0x20, item->QueryAddressByName(oui::String(ORTHIA_TCSTR("fl_10100000!$entry_0002")), 0));
    EXPECT_EQ(FloatingFixture::regionA, item->QueryAddressByName(oui::String(ORTHIA_TCSTR("fl_10100000")), 0));
    EXPECT_EQ(FloatingFixture::regionA, item->GetRangeInfo(FloatingFixture::regionA + 0x10).address);

    // a discovery again finds nothing new
    EXPECT_TRUE(orthia::DiscoverFloatingCode(*item, source));
    EXPECT_EQ(1u, orthia::QueryPendingFloatingModules(*item).size());

    // the analysis of A leads to D
    FloatingFixture::AnalyzePending(*item);
    const auto analyzedA = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
    EXPECT_TRUE(analyzedA.flags & orthia::ModuleInfo::flags_analyzeDone);
    EXPECT_EQ(FloatingFixture::regionA, analyzedA.floating->codeStart);
    EXPECT_EQ(FloatingFixture::regionA + 0x20, analyzedA.floating->codeLast);
    const auto d = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
    ASSERT_TRUE(d.IsFloating());
    EXPECT_TRUE(d.flags & orthia::ModuleInfo::flags_analyzeDone);
    ASSERT_EQ(1u, d.floating->entries.size());
    EXPECT_EQ((std::vector<orthia::Address_type>{ FloatingFixture::regionA }), d.floating->entries[0].referrers);
    EXPECT_TRUE(orthia::QueryPendingFloatingModules(*item).empty());

    std::vector<orthia::PlatformString_type> lines;
    orthia::DescribeFloatingModule(*item, analyzedA, lines);
    std::string text;
    for (const auto& line : lines)
    {
        text += orthia::PlatformStringToUtf8(line) + "\n";
    }
    EXPECT_NE(std::string::npos, text.find("Floating code: RWX, private")) << text;
    EXPECT_NE(std::string::npos, text.find("$entry_0002")) << text;
    EXPECT_NE(std::string::npos, text.find("from ")) << text;
    EXPECT_NE(std::string::npos, text.find("Entries: 2")) << text;

    // lmf /v: by referrer, the module's two calls first, then A's jump to D
    lines.clear();
    orthia::DescribeFloatingReferrers(*item, lines);
    ASSERT_EQ(4u, lines.size());
    EXPECT_EQ(ORTHIA_TCSTR("Referrers:"), lines[0]);
    auto row = [&](size_t i) { return orthia::PlatformStringToUtf8(lines[i]); };
    EXPECT_NE(std::string::npos, row(1).find("-> fl_10100000!$entry_0001")) << row(1);
    EXPECT_NE(std::string::npos, row(1).find("RWX private [function start]")) << row(1);
    EXPECT_NE(std::string::npos, row(2).find("module+05h")) << row(2);
    EXPECT_NE(std::string::npos, row(2).find("-> fl_10100000!$entry_0002")) << row(2);
    EXPECT_EQ(std::string::npos, row(2).find("[function start]")) << row(2);
    EXPECT_EQ(0u, row(3).find("  fl_10100000!$entry_0001")) << row(3);
    EXPECT_NE(std::string::npos, row(3).find("-> fl_10400000!$entry_0001")) << row(3);
}

// an import slot of the module that leads into memory no module owns: an entry with the slot,
// listed by lmf /v; slots that stay in the module or lead to another one don't count
TEST(FloatingCode, PointerSlots)
{
    FloatingFixture fixture;
    auto item = fixture.OpenItem();
    fixture.AnalyzeModule(*item);
    const auto source = FloatingFixture::FindModule(*item, FloatingFixture::moduleBase);

    auto makeSlot = [](orthia::Address_type slot, orthia::Address_type value, const char* name) {
        orthia::PointerSlotInfo info;
        info.slot = slot;
        info.value = value;
        info.kind = orthia::PointerSlotInfo::kind_import;
        info.name = name;
        return info;
    };
    std::vector<orthia::PointerSlotInfo> slots = {
        makeSlot(FloatingFixture::moduleBase + 0x800, FloatingFixture::regionD + 0x10, "KERNEL32.dll!CreateFileW"),
        makeSlot(FloatingFixture::moduleBase + 0x808, FloatingFixture::otherModule, "other!f"),
        makeSlot(FloatingFixture::moduleBase + 0x810, FloatingFixture::moduleBase + 0x20, "own"),
        makeSlot(FloatingFixture::moduleBase + 0x818, FloatingFixture::regionData, "data"),
    };
    std::vector<orthia::ModuleInfo> modules;
    item->GetModules(modules);
    auto found = orthia::CollectFloatingRegions(*fixture.moduleManager, *fixture.process, modules, source, slots);

    std::shared_ptr<orthia::FloatingRegionInfo> d;
    for (const auto& info : found.changed)
    {
        if (info->allocationBase == FloatingFixture::regionD)
            d = info;
    }
    ASSERT_TRUE(d);
    ASSERT_EQ(1u, d->entries.size());
    EXPECT_EQ(FloatingFixture::regionD + 0x10, d->entries[0].address);
    EXPECT_TRUE(d->entries[0].referrers.empty());
    ASSERT_EQ(1u, d->entries[0].slots.size());
    EXPECT_EQ(FloatingFixture::moduleBase + 0x800, d->entries[0].slots[0].slot);
    EXPECT_EQ("KERNEL32.dll!CreateFileW", d->entries[0].slots[0].name);
    EXPECT_NE(found.toAnalyze.end(), std::find(found.toAnalyze.begin(), found.toAnalyze.end(), FloatingFixture::regionD));
    // the other slots: nothing
    for (const auto& info : found.changed)
    {
        for (const auto& entry : info->entries)
        {
            if (info != d)
                EXPECT_TRUE(entry.slots.empty());
        }
    }

    // saved and named
    orthia::SaveFloatingRegion(*fixture.moduleManager, *d);
    item->SetFloatingModule(d);
    std::vector<orthia::PlatformString_type> lines;
    orthia::DescribeFloatingReferrers(*item, lines);
    std::string text;
    for (const auto& line : lines)
    {
        text += orthia::PlatformStringToUtf8(line) + "\n";
    }
    EXPECT_NE(std::string::npos, text.find("module+0800h")) << text;
    EXPECT_NE(std::string::npos, text.find("-> fl_10400000!$entry_0001")) << text;
    EXPECT_NE(std::string::npos, text.find("RWX private [import slot: KERNEL32.dll!CreateFileW]")) << text;

    lines.clear();
    orthia::DescribeFloatingModule(*item, FloatingFixture::FindModule(*item, FloatingFixture::regionD), lines);
    text.clear();
    for (const auto& line : lines)
    {
        text += orthia::PlatformStringToUtf8(line) + "\n";
    }
    EXPECT_NE(std::string::npos, text.find("from module+0800h (import slot: KERNEL32.dll!CreateFileW)")) << text;
}

TEST(FloatingCode, SavedAndStale)
{
    FloatingFixture fixture;
    {
        auto item = fixture.OpenItem();
        fixture.AnalyzeModule(*item);
        orthia::DiscoverFloatingCode(*item, FloatingFixture::FindModule(*item, FloatingFixture::moduleBase));
        FloatingFixture::AnalyzePending(*item);
    }

    // the same process opened again: as the database reuse does
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        item->LoadSavedModuleFlags();
        const auto a = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
        ASSERT_TRUE(a.IsFloating());
        EXPECT_TRUE(a.flags & orthia::ModuleInfo::flags_analyzeDone);
        EXPECT_FALSE(a.flags & orthia::ModuleInfo::flags_stale);
        EXPECT_EQ(2u, a.floating->entries.size());
        EXPECT_EQ(FloatingFixture::regionA + 0x20, a.floating->codeLast);
        EXPECT_TRUE(FloatingFixture::FindModule(*item, FloatingFixture::regionD).IsFloating());
        EXPECT_TRUE(orthia::QueryPendingFloatingModules(*item).empty());
    }

    // D changed: kept and stale, not analyzed again
    fixture.process->regions.back().data[1] = (char)0x90;
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        item->LoadSavedModuleFlags();
        const auto d = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
        ASSERT_TRUE(d.IsFloating());
        EXPECT_TRUE(d.flags & orthia::ModuleInfo::flags_stale);
        EXPECT_FALSE(FloatingFixture::FindModule(*item, FloatingFixture::regionA).flags & orthia::ModuleInfo::flags_stale);
    }

    // A gone: stale too
    fixture.process->regions.erase(fixture.process->regions.begin());
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        EXPECT_TRUE(FloatingFixture::FindModule(*item, FloatingFixture::regionA).flags & orthia::ModuleInfo::flags_stale);
    }
}

// .analyze /f on a stale module: analyzed again from the memory as it is now
TEST(FloatingCode, StaleRefreshed)
{
    FloatingFixture fixture;
    {
        auto item = fixture.OpenItem();
        fixture.AnalyzeModule(*item);
        orthia::DiscoverFloatingCode(*item, FloatingFixture::FindModule(*item, FloatingFixture::moduleBase));
        FloatingFixture::AnalyzePending(*item);
    }

    // D now has more code: c3 at +0 becomes nop, so the code runs on to the ret at +1
    fixture.process->regions.back().data[0] = (char)0x90;
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        item->LoadSavedModuleFlags();
        auto d = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
        ASSERT_TRUE(d.flags & orthia::ModuleInfo::flags_stale);
        EXPECT_EQ(FloatingFixture::regionD, d.floating->codeLast);

        ASSERT_EQ(orthia::FloatingRefresh::refreshed, orthia::RefreshStaleFloatingModule(*item, d));
        EXPECT_EQ(FloatingFixture::regionD, d.address);
        const auto pending = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
        EXPECT_FALSE(pending.flags & (orthia::ModuleInfo::flags_stale | orthia::ModuleInfo::flags_analyzeDone));
        // the entries and their referrers stay
        ASSERT_EQ(1u, pending.floating->entries.size());
        EXPECT_EQ((std::vector<orthia::Address_type>{ FloatingFixture::regionA }), pending.floating->entries[0].referrers);

        FloatingFixture::AnalyzePending(*item);
        const auto analyzed = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
        EXPECT_TRUE(analyzed.flags & orthia::ModuleInfo::flags_analyzeDone);
        EXPECT_EQ(FloatingFixture::regionD + 1, analyzed.floating->codeLast);
    }

    // saved as it is now: not stale on the next open
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        item->LoadSavedModuleFlags();
        const auto d = FloatingFixture::FindModule(*item, FloatingFixture::regionD);
        ASSERT_TRUE(d.IsFloating());
        EXPECT_FALSE(d.flags & orthia::ModuleInfo::flags_stale);
        EXPECT_TRUE(d.flags & orthia::ModuleInfo::flags_analyzeDone);
        EXPECT_EQ(FloatingFixture::regionD + 1, d.floating->codeLast);
    }
}

// .analyze /f on a stale module whose memory went away: the module goes too
TEST(FloatingCode, StaleRemoved)
{
    FloatingFixture fixture;
    {
        auto item = fixture.OpenItem();
        fixture.AnalyzeModule(*item);
        orthia::DiscoverFloatingCode(*item, FloatingFixture::FindModule(*item, FloatingFixture::moduleBase));
        FloatingFixture::AnalyzePending(*item);
    }
    // D freed; A stays but is no longer executable
    fixture.process->regions.pop_back();
    fixture.process->regions.front().protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write;
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        for (auto address : { FloatingFixture::regionD, FloatingFixture::regionA })
        {
            auto mod = FloatingFixture::FindModule(*item, address);
            ASSERT_TRUE(mod.flags & orthia::ModuleInfo::flags_stale);
            EXPECT_EQ(orthia::FloatingRefresh::removed, orthia::RefreshStaleFloatingModule(*item, mod));
            EXPECT_FALSE(FloatingFixture::FindModule(*item, address).IsFloating());
        }
    }
    {
        auto item = fixture.OpenItem();
        item->LoadSavedFloatingModules();
        EXPECT_FALSE(FloatingFixture::FindModule(*item, FloatingFixture::regionA).IsFloating());
        EXPECT_FALSE(FloatingFixture::FindModule(*item, FloatingFixture::regionD).IsFloating());
        EXPECT_FALSE(fixture.moduleManager->QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(FloatingFixture::regionA));
    }
}

// the fingerprint covers the executable pages: data next to the code may change
TEST(FloatingCode, HashCoversExecutablePages)
{
    CFakeProcess process;
    const orthia::Address_type base = 0x20000000;
    CFakeProcess::Region code;
    code.base = base;
    code.allocationBase = base;
    code.data.assign(0x1000, (char)0xC3);
    process.regions.push_back(code);
    CFakeProcess::Region data;
    data.base = base + 0x1000;
    data.allocationBase = base;
    data.data.assign(0x2000, 0);
    data.protect = oui::MemoryRegionInfo::protect_read | oui::MemoryRegionInfo::protect_write;
    process.regions.push_back(data);

    orthia::FloatingRegionInfo info;
    info.allocationBase = base;
    info.allocationSize = 0x3000;
    orthia::FingerprintFloatingRegion(process, info);
    EXPECT_EQ(0x1000u, info.hashedSize);
    EXPECT_TRUE(orthia::IsFloatingRegionUnchanged(process, info));

    process.regions[1].data[0x10] = 1;
    EXPECT_TRUE(orthia::IsFloatingRegionUnchanged(process, info));

    process.regions[0].data[0x10] = (char)0x90;
    EXPECT_FALSE(orthia::IsFloatingRegionUnchanged(process, info));
    process.regions[0].data[0x10] = (char)0xC3;
    EXPECT_TRUE(orthia::IsFloatingRegionUnchanged(process, info));

    // the data part made executable: another fingerprint
    process.regions[1].protect |= oui::MemoryRegionInfo::protect_exec;
    EXPECT_FALSE(orthia::IsFloatingRegionUnchanged(process, info));
}

TEST(Sha256, KnownValues)
{
    EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", orthia::Sha256Hex("", 0));
    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", orthia::Sha256Hex("abc", 3));
    const std::string twoBlocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    EXPECT_EQ("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
        orthia::Sha256Hex(twoBlocks.data(), twoBlocks.size()));
    // in pieces that don't fall on the blocks
    const std::string million(1000000, 'a');
    orthia::CSha256 sha;
    for (size_t offset = 0; offset < million.size(); offset += 999)
    {
        sha.Update(million.data() + offset, std::min<size_t>(999, million.size() - offset));
    }
    EXPECT_EQ("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", sha.FinishHex());
}

// the bytes are saved when a region is found; the folder that holds them stays when the process is gone
TEST(FloatingCode, Snapshot)
{
    namespace fs = std::filesystem;
    FloatingFixture fixture;
    const fs::path folder = fixture.root / ORTHIA_TCSTR("[1] fake");
    fs::create_directories(folder);
    std::string firstFile;
    {
        auto item = fixture.OpenItem();
        item->SetProcFolder(folder.native());
        fixture.AnalyzeModule(*item);
        orthia::DiscoverFloatingCode(*item, FloatingFixture::FindModule(*item, FloatingFixture::moduleBase));
        FloatingFixture::AnalyzePending(*item);

        const auto a = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
        ASSERT_TRUE(a.floating);
        const auto& info = *a.floating;
        ASSERT_FALSE(info.snapshotFile.empty());
        EXPECT_EQ(0u, info.snapshotFile.find("10100000_"));
        EXPECT_EQ(0x1000u, info.snapshotSize);
        EXPECT_EQ(0u, info.snapshotMissing);
        firstFile = info.snapshotFile;

        // the file holds the bytes of the region, and its hash is the one recorded
        const fs::path path = folder / orthia::g_floatingSnapshotFolder / orthia::Utf8ToPlatformString(info.snapshotFile);
        std::ifstream file(path, std::ios::binary);
        std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        EXPECT_EQ(fixture.process->regions.front().data, bytes);
        EXPECT_EQ(orthia::Sha256Hex(bytes.data(), bytes.size()), info.sha256);
        EXPECT_TRUE(orthia::HasFloatingSnapshots(folder.native()));
        // D, found by the analysis of A, has one too
        EXPECT_FALSE(FloatingFixture::FindModule(*item, FloatingFixture::regionD).floating->snapshotFile.empty());

        std::vector<orthia::PlatformString_type> lines;
        orthia::DescribeFloatingModule(*item, a, lines);
        std::string text;
        for (const auto& line : lines)
        {
            text += orthia::PlatformStringToUtf8(line) + "\n";
        }
        EXPECT_NE(std::string::npos, text.find("Snapshot: ")) << text;
        EXPECT_NE(std::string::npos, text.find(info.snapshotFile + " (1000 bytes)")) << text;
        EXPECT_NE(std::string::npos, text.find("SHA-256: " + info.sha256)) << text;
    }
    // the fake process doesn't run: without the snapshots the folder would be gone
    ASSERT_TRUE(fs::is_directory(folder));

    // reopened: the snapshot is in the saved record. A changed and refreshed: a second file, the first stays
    fixture.process->regions.front().data[0x40] = (char)0x90;
    {
        auto item = fixture.OpenItem();
        item->SetProcFolder(folder.native());
        item->LoadSavedFloatingModules();
        auto a = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
        EXPECT_EQ(firstFile, a.floating->snapshotFile);
        ASSERT_TRUE(a.flags & orthia::ModuleInfo::flags_stale);
        ASSERT_EQ(orthia::FloatingRefresh::refreshed, orthia::RefreshStaleFloatingModule(*item, a));
        const auto refreshed = FloatingFixture::FindModule(*item, FloatingFixture::regionA);
        EXPECT_NE(firstFile, refreshed.floating->snapshotFile);
        EXPECT_TRUE(fs::exists(folder / orthia::g_floatingSnapshotFolder / orthia::Utf8ToPlatformString(firstFile)));
        EXPECT_TRUE(fs::exists(folder / orthia::g_floatingSnapshotFolder / orthia::Utf8ToPlatformString(refreshed.floating->snapshotFile)));
    }

    // a folder without snapshots goes when its process is gone, as before
    const fs::path other = fixture.root / ORTHIA_TCSTR("[2] fake");
    fs::create_directories(other);
    {
        auto item = fixture.OpenItem();
        item->SetProcFolder(other.native());
    }
    EXPECT_FALSE(fs::exists(other));
}

int RunTests()
{
    // --run-tests comes before the app's own init: the analysis needs the decoder tables
    Diana_Init();
    testing::internal::CaptureStderr();
    testing::internal::GetCapturedStderr();
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}