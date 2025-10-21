#include "orthia_model_interfaces.h"

namespace orthia
{

    void ÑPeristentItemStorage::AsyncQueryGotoInfo(ThreadPtr_type targetThread,
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
            items.push_back(pair.second);
        }
        std::sort(items.begin(), items.end(), [](auto& o1, auto& o2) {  return o1.lastUpdateTime.ToLongLongTime() > o2.lastUpdateTime.ToLongLongTime(); });
        int error = 0;
        filterHandler->Reply(filterHandler, filter, items, error);
    }

    void ÑPeristentItemStorage::AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
        oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
        orthia::Address_type address)
    {
        auto pair = m_dataItems.insert(std::make_pair(address, GotoItem(address)));
        if (!pair.second)
        {
            pair.first->second.lastUpdateTime.InitFromCurrentTime();
        }
        int error = 0;
        gotoHandler->Reply(address, error);
    }

}