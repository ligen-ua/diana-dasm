#include "oui_layouts.h"
#include "oui_containers.h"

namespace oui
{
    CLayoutIterator::CLayoutIterator(int groupsCount)
    {
        // there is a tradeoff here,
        // we should never overflow vector's capacity 
        // because of iterators are stored inside and can be invalidated
        const int defValue = 256;
        if (groupsCount < defValue)
        {
            groupsCount = 256;
        }
        m_states.reserve(groupsCount);
    }
    CLayoutIterator::~CLayoutIterator()
    {
    }
    void CLayoutIterator::InitStart(std::shared_ptr<PanelLayout> layout)
    {
        m_states.clear();
        m_flags = flag_start;
        m_states.push_back(LayoutIteratorState(layout));
    }
    void CLayoutIterator::InitEnd(std::shared_ptr<PanelLayout> layout)
    {
        m_states.clear();
        m_flags = flag_end;
        m_states.push_back(LayoutIteratorState(layout));
    }
    std::shared_ptr<PanelLayout> CLayoutIterator::GetLayout()
    {
        if (m_flags & (flag_start | flag_end))
        {
            return 0;
        }

        auto& state = m_states.back();
        return state.layout;
    }
    bool CLayoutIterator::ScanForLeftLeaf()
    {
        for (;;)
        {
            auto& state = m_states.back();
            if (state.layout->group || state.layout->data.empty())
            {
                return true;
            }
            state.SetIterator(state.layout->data.begin());
            m_states.push_back(LayoutIteratorState(*state.it));
        }
    }
    bool CLayoutIterator::MoveNext()
    {
        if (m_states.empty() || (m_flags & flag_end))
        {
            return false;
        }

        if (m_flags & flag_start)
        {
            m_flags = 0;
            return ScanForLeftLeaf();
        }
        for (; !m_states.empty(); )
        {
            auto last = m_states.rbegin();
            if (last->HasValidIterator())
            {
                // try next
                auto oldIt = last->it++;
                if (last->it != last->layout->data.end())
                {
                    m_states.push_back(LayoutIteratorState(*last->it));
                    return ScanForLeftLeaf();
                }
            }
            OnStateDone();
            m_states.pop_back();
        }
        m_flags |= flag_end;
        return false;
    }

    bool CLayoutIterator::ScanForRightLeaf()
    {
        for (;;)
        {
            auto& state = m_states.back();
            if (state.layout->group || state.layout->data.empty())
            {
                return true;
            }
            state.SetIterator(--state.layout->data.end());
            m_states.push_back(LayoutIteratorState(*state.it));
        }
    }
    bool CLayoutIterator::MovePrev()
    {
        if (m_states.empty() || (m_flags & flag_start))
        {
            return false;
        }
        if (m_flags & flag_end)
        {
            m_flags = 0;
            return ScanForRightLeaf();
        }
        for (; !m_states.empty(); )
        {
            auto last = m_states.rbegin();
            if (last->HasValidIterator())
            {
                // try next
                if (last->it != last->layout->data.begin())
                {
                    auto prevIt = last->it--;
                    m_states.push_back(LayoutIteratorState(*last->it));
                    return ScanForRightLeaf();
                }
            }
            OnStateDone();
            m_states.pop_back();
        }
        m_flags |= flag_end;
        return false;
    }
    bool CLayoutIterator::IsValid() const
    {
        return m_flags == 0;
    }



}