#include "gtest/gtest.h"
#include "oui_containers.h"
#include "orthia_utils.h"
#include "oui_layouts_calc.h"
#include "orthia_databases.h"
#include "orthia_module_names.h"
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

    std::vector<orthia::DatabaseInfo> all = { fresh, broken, brokenNew, oldFile, pending,
        alive, gone, reused, unknown, expired, noPid, openGone, openBroken };
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

int RunTests()
{
    testing::internal::CaptureStderr();
    testing::internal::GetCapturedStderr();
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}