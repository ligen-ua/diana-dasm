#include "gtest/gtest.h"
#include "oui_containers.h"
#include "orthia_utils.h"
#include "oui_layouts_calc.h"

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

int RunTests()
{
    testing::internal::CaptureStderr();
    testing::internal::GetCapturedStderr();
    ::testing::InitGoogleTest();
    return RUN_ALL_TESTS();
}