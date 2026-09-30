#include "gtest/gtest.h"
#include "oui_containers.h"
#include "orthia_utils.h"
#include "oui_layouts_calc.h"
#include "orthia_databases.h"
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

int RunTests()
{
    testing::internal::CaptureStderr();
    testing::internal::GetCapturedStderr();
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}