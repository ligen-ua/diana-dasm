#pragma once

#include "oui_base.h"

namespace oui
{
    class CPanelGroupWindow;

    extern int g_defaultPreferredWidth;
    extern int g_defaultPreferredHeight;

    // group info
    struct GroupInfo
    {
        Size preferredSize = { g_defaultPreferredWidth, g_defaultPreferredHeight };
        bool stretchWidth = false;
        bool stretchHeight = false;
        GroupInfo()
        {
        }
        struct StretchAll
        {
        };
        GroupInfo(StretchAll)
        {
            stretchWidth = true;
            stretchHeight = true;
        }
        GroupInfo(int preferredWidth,
            int preferredHeight)
        {
            preferredSize = Size{ preferredWidth, preferredHeight };
        }
    };

    // main container window
    enum class PanelItemType
    {
        None,
        Vertical,
        Horizontal
    };
    struct PanelLayout
    {
        using List_type = std::list<std::shared_ptr<PanelLayout>>;

        PanelItemType type = PanelItemType::None;
        std::shared_ptr<CPanelGroupWindow> group;
        List_type data;

        bool stretchWidth = false;
        bool stretchHeight = false;
        Rect rect;
        std::weak_ptr<PanelLayout> parentLayout;

        PanelLayout()
        {
        }
        PanelLayout(std::shared_ptr<CPanelGroupWindow> group_in)
            :
            group(group_in)
        {
        }
        void ResetType(PanelItemType newType)
        {
            if (type == PanelItemType::None &&
                newType != PanelItemType::None)
            {
                if (group)
                {
                    data.push_back(std::make_shared<PanelLayout>(group));
                    group = 0;
                }
            }
            type = newType;
        }
    };

    struct LayoutIteratorState
    {
        static const int flag_iterator_valid = 1;

        int flags = 0;
        std::shared_ptr<PanelLayout> layout;
        PanelLayout::List_type::iterator it;

        LayoutIteratorState()
        {
        }
        explicit LayoutIteratorState(std::shared_ptr<PanelLayout> layout_in)
            :
                layout(layout_in)
        {
        }
        bool HasValidIterator() const
        {
            return (flags & flag_iterator_valid);
        }
        void SetIterator(PanelLayout::List_type::iterator it_in)
        {
            it = it_in;
            flags |= flag_iterator_valid;
        }
    };
    class CLayoutIterator
    {
    protected:
        static const int flag_start = 1;
        static const int flag_end = 2;

        int m_flags = 0;
        std::vector<LayoutIteratorState> m_states;
       
        bool ScanForLeftLeaf();
        bool ScanForRightLeaf();

        virtual void OnStateDone() {}
    public:
        CLayoutIterator(int groupsCount = 0);
        virtual ~CLayoutIterator();
        void InitStart(std::shared_ptr<PanelLayout> layout);
        void InitEnd(std::shared_ptr<PanelLayout> layout);

        std::shared_ptr<PanelLayout> GetLayout();

        bool MovePrev();
        bool MoveNext();
        bool IsValid() const;
    };

}