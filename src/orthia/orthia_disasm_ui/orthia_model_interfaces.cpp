#include "orthia_model_interfaces.h"

namespace orthia
{

    void ÑPersistentItemStorage::AsyncQueryGotoInfo(ThreadPtr_type targetThread,
        const oui::String& filter,
        oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
        int flags)
    {
        auto filterLowercase = orthia::Downcase(filter.native);
        std::wstring text;

        std::vector<GotoItem> items;
        for (auto& pair : m_dataItems)
        {
            if (!filterLowercase.empty())
            {
                text = orthia::ToWideStringAsHex(pair.first);
                text.append(L"|");
                text.append(pair.second.comment.native);

                text = orthia::Downcase(text);
                auto substr = text.find(filterLowercase.c_str(), 0);
                if (substr == text.npos)
                {
                    continue;
                }
            }
            if (!(flags & goto_flags_history_mode))
            {
                if (pair.second.flags & goto_flags_history_mode)
                {
                    continue;
                }
            }
            items.push_back(pair.second);
        }
        std::sort(items.begin(), items.end(), [](auto& o1, auto& o2) {  return o1.lastUpdateTime.ToLongLongTime() > o2.lastUpdateTime.ToLongLongTime(); });
        int error = 0;
        filterHandler->Reply(filterHandler, filter, items, error);
    }

    void ÑPersistentItemStorage::AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
        oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
        orthia::Address_type address,
        int flags,
        orthia::Address_type pageAddress)
    {
        auto pair = m_dataItems.insert(std::make_pair(address, GotoItem(address, flags)));
        if (!pair.second)
        {
            pair.first->second.lastUpdateTime.InitFromCurrentTime();
        }

        if (flags & goto_flags_history_mode)
        {
            if (m_historyIndex == -1)
            {
                m_history.clear();
            }
            else
            {
                if ((m_historyIndex + 1) < m_history.size())
                {
                    m_history.erase(m_history.begin() + m_historyIndex + 1, m_history.end());
                }
            }
            ++m_historyIndex;
            m_history.push_back(HistoryGotoItem(address, flags, pageAddress));
        }

        int error = 0;
        gotoHandler->ReplyWithRetain(gotoHandler, address, error);
    }

    void ÑPersistentItemStorage::AsyncFetchPrevHistory(ThreadPtr_type targetThread,
        oui::OperationPtr_type<FetchCompleteHandler_type> gotoHandler)
    {
        orthia::Address_type address = 0;
        orthia::Address_type pageAddress = 0;
        int error = DI_NOT_FOUND;
        if (m_historyIndex != -1 && m_historyIndex < m_history.size())
        {
            address = m_history[m_historyIndex].address;
            pageAddress = m_history[m_historyIndex].pageAddress;
            error = 0;
            --m_historyIndex;
        }
        gotoHandler->ReplyWithRetain(gotoHandler, address, error, pageAddress);
    }


}