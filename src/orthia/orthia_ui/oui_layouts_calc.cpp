#include "oui_layouts_calc.h"
#include "oui_containers.h"

namespace oui
{
    const int g_maxRecursionLevel = 100;

    static
    void RepositionChilds(std::shared_ptr<PanelLayout> layout, const Rect& wndRect,
        bool applyStretchWidth,
        bool applyStretchHeight,
        const int level)
    {
        if (level >= g_maxRecursionLevel)
        {
            return;
        }
        if (layout->group)
        {
            const auto groupInfo = layout->group->Info();
            auto sizeToUse = groupInfo.preferredSize;
            if (applyStretchHeight || sizeToUse.height > wndRect.size.height)
            {
                sizeToUse.height = wndRect.size.height;
            }

            if (applyStretchWidth || sizeToUse.width > wndRect.size.width)
            {
                sizeToUse.width = wndRect.size.width;
            }
            layout->group->SetLeftBorderState(wndRect.position.x != 0);
            layout->rect.size = sizeToUse;
            layout->rect.position = wndRect.position;
            return;
        }
        const int newLevel = level + 1;
        Rect slidingRect = wndRect;
        if (layout->type == PanelItemType::Horizontal)
        {
            // go ltr
            auto it = layout->data.begin();
            const auto it_end = layout->data.end();
            for (;;)
            {
                auto child = *it;
                if (child->stretchWidth)
                {
                    break;
                }
                RepositionChilds(child, slidingRect, false, true, newLevel);
                ++it;
                if (it == it_end)
                {
                    break;
                }
                slidingRect.position.x += child->rect.size.width;
                slidingRect.size.width -= child->rect.size.width;
            }
            auto reposEnd = it;
            if (reposEnd == layout->data.end())
            {
                --reposEnd;
            }
            // here reposEnd points to someone we want to be stretched
            // go rtl
            it = it_end;
            --it;
            for (;
                it != reposEnd;
                --it)
            {
                auto child = *it;
                RepositionChilds(child, slidingRect, false, true, newLevel);

                Rect renderRect = slidingRect;
                renderRect.position.x = slidingRect.position.x + slidingRect.size.width - child->rect.size.width;
                renderRect.size.width = child->rect.size.width;

                RepositionChilds(child, renderRect, false, true, newLevel);

                // apply slidingRect
                slidingRect.size.width -= child->rect.size.width;
            }
            RepositionChilds(*reposEnd, slidingRect, true, true, newLevel);
        }
        else
        {
            // vertical
            // go ttb
            auto it = layout->data.begin();
            const auto it_end = layout->data.end();
            for (;;)
            {
                auto child = *it;
                if (child->stretchHeight)
                {
                    break;
                }
                RepositionChilds(child, slidingRect, true, false, newLevel);
                ++it;
                if (it == it_end)
                {
                    break;
                }
                slidingRect.position.y += child->rect.size.height;
                slidingRect.size.height -= child->rect.size.height;
            }
            auto reposEnd = it;
            if (reposEnd == layout->data.end())
            {
                --reposEnd;
            }
            // here reposEnd points to someone we want to be stretched
            // go btt
            it = it_end;
            --it;
            for (;
                it != reposEnd;
                --it)
            {
                auto child = *it;
                RepositionChilds(child, slidingRect, true, false, newLevel);

                Rect renderRect = slidingRect;
                renderRect.position.y = slidingRect.position.y + slidingRect.size.height - child->rect.size.height;
                renderRect.size.height = child->rect.size.height;

                RepositionChilds(child, renderRect, true, false, newLevel);

                // apply slidingRect
                slidingRect.size.height -= child->rect.size.height;
            }
            RepositionChilds(*reposEnd, slidingRect, true, true, newLevel);
        }
        layout->rect = wndRect;
    }


    void CalculateStretchLayout(std::shared_ptr<PanelLayout> layout, 
        std::shared_ptr<PanelLayout> parentLayout, 
        const int level)
    {
        if (layout->group)
        {
            const auto groupInfo = layout->group->Info();
            layout->stretchWidth = groupInfo.stretchWidth;
            layout->stretchHeight = groupInfo.stretchHeight;
            layout->parentLayout = parentLayout;
            return;
        }
        layout->stretchWidth = false;
        layout->stretchHeight = false;
        const int newLevel = level + 1;

        for (auto it = layout->data.begin(), it_end = layout->data.end();
            it != it_end;
            ++it)
        {
            auto child = *it;
            child->parentLayout = layout;

            CalculateStretchLayout(child, layout, newLevel);

            if (child->stretchWidth)
                layout->stretchWidth = true;

            if (child->stretchHeight)
                layout->stretchHeight = true;

        }
    }

    void RepositionLayout(std::shared_ptr<PanelLayout> layout, const Rect& wndRect, bool clearCache, bool moveGroups)
    {
        if (clearCache)
        {
            CalculateStretchLayout(layout, nullptr, 0);
        }

        RepositionChilds(layout, wndRect, true, true, 0);

        if (moveGroups)
        {
            // move groups too
            oui::CLayoutIterator iterator;
            iterator.InitStart(layout);
            for (; iterator.MoveNext();)
            {
                auto layout = iterator.GetLayout();
                layout->group->SetLayout(layout);
                layout->group->MoveTo(layout->rect.position);
                layout->group->Resize(layout->rect.size);
            }
        }
    }
    
    void ResizeLayoutY(std::shared_ptr<PanelLayout> layout, int newHeight, bool top)
    {
        if (layout->group)
        {
            layout->rect.size.height = newHeight;
            layout->group->SetPreferredSize(layout->rect.size);
            return;
        }
    }
    void ResizeLayoutX(std::shared_ptr<PanelLayout> layout, int newWidth, bool left)
    {
        if (layout->group)
        {
            layout->rect.size.width = newWidth;
            layout->group->SetPreferredSize(layout->rect.size);
            return;
        }
    }

}