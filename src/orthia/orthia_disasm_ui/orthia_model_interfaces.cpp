#include "orthia_model_interfaces.h"

namespace orthia
{

    void CPersistentItemStorage::AsyncQueryGotoInfo(ThreadPtr_type targetThread,
        const oui::String& filter,
        oui::OperationPtr_type<QueryGotoItemHandler_type> filterHandler,
        int flags)
    {
        orthia::CAutoCriticalSection guard(m_lock);

        auto filterLowercase = orthia::Downcase(filter.native);
        orthia::PlatformString_type text;

        std::vector<GotoItem> items;
        for (auto& pair : m_dataItems)
        {
            if (!filterLowercase.empty())
            {
                text = orthia::ToWideStringAsHex(pair.first);
                text.append(ORTHIA_TCSTR("|"));
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

    void CPersistentItemStorage::AsyncUpdateGotoInfo(ThreadPtr_type targetThread,
        oui::OperationPtr_type<GotoCompleteHandler_type> gotoHandler,
        orthia::Address_type address,
        int flags,
        orthia::Address_type pageAddress)
    {
        orthia::CAutoCriticalSection guard(m_lock);

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
                if ((m_historyIndex + 1) < (int)m_history.size())
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

    void CPersistentItemStorage::AsyncFetchPrevHistory(ThreadPtr_type targetThread,
        oui::OperationPtr_type<FetchCompleteHandler_type> gotoHandler)
    {
        orthia::CAutoCriticalSection guard(m_lock);

        orthia::Address_type address = 0;
        orthia::Address_type pageAddress = 0;
        int error = DI_NOT_FOUND;
        if (m_historyIndex != -1 && m_historyIndex < (int)m_history.size())
        {
            address = m_history[m_historyIndex].address;
            pageAddress = m_history[m_historyIndex].pageAddress;
            error = 0;
            --m_historyIndex;
        }
        gotoHandler->ReplyWithRetain(gotoHandler, address, error, pageAddress);
    }

    oui::String CPersistentItemStorage::SyncReadComment(orthia::Address_type address)
    {
        orthia::CAutoCriticalSection guard(m_lock);

        auto it = m_comments.find(address);
        if (it == m_comments.end())
        {
            return oui::String();
        }
        return it->second.text;
    }
    oui::fsui::OpenResult CPersistentItemStorage::SyncWriteComment(orthia::Address_type address, const oui::String& comment)
    {
        orthia::CAutoCriticalSection guard(m_lock);

        m_comments[address].text = comment;
        return oui::fsui::OpenResult();
    }

    oui::String ComposeName(const oui::String& name, Address_type nameAddress, Address_type address)
    {
        Address_type diff = address - nameAddress;
        if (!diff)
        {
            return name;
        }
        oui::String addressStr;
        if (diff > std::numeric_limits<uint32_t>::max())
        {
            orthia::ToStringAsHex(diff, &addressStr.native);
        }
        else if (diff > std::numeric_limits<uint16_t>::max())
        {
            orthia::ToStringAsHex((uint32_t)diff, &addressStr.native);
        }
        else if (diff > std::numeric_limits<uint8_t>::max())
        {
            orthia::ToStringAsHex((uint16_t)diff, &addressStr.native);
        }
        else
        { 
            orthia::ToStringAsHex((uint8_t)diff, &addressStr.native);
        }
    
        oui::String result;
        result.native = name.native + OUI_TCSTR("+");
        if (addressStr.native.front() >= OUI_TCSTR('a') && addressStr.native.front() <= OUI_TCSTR('z'))
        {
            result.native += OUI_TCSTR("0");
        }
        result.native += addressStr.native;
        result.native += OUI_TCSTR("h");
        return result;
    }
}