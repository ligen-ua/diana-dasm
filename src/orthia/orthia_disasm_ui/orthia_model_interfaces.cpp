#include "orthia_model_interfaces.h"
#include "orthia_database_module.h"
#include "orthia_module_manager.h"
#include "oui_disasm_colors.h"

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

    void AppendXrefLine(Address_type address, IMarkupCache* cache, CModuleManager* moduleManager, int dianaMode, std::vector<MarkupLine>& allLines)
    {
        if (!moduleManager)
            return;
        const CommonReferenceInfoArray_type* refsPtr = nullptr;
        std::vector<CommonReferenceInfo> refsStorage;
        if (!cache)
        {
            moduleManager->QueryReferencesToInstruction(address, &refsStorage);
            refsPtr = &refsStorage;
        }
        else
        {
            cache->QueryReferences(address, &refsPtr);
        }
        if (refsPtr && !refsPtr->empty())
        {
            oui::DisasmColorsProfile colors;
            oui::QueryDefaultColorProfile(colors);
            oui::CTextMarkupBuilder builder;

            PlatformString_type xrefText;

            const PlatformString_type prefix = OUI_TCSTR(" <-- ");
            xrefText += prefix;
            builder.AddNextRange(prefix.size(), colors.bytes);

            const int itemsToWrite = 1;
            for (int i = 0; ; )
            {
                if (i >= refsPtr->size())
                {
                    break;
                }
                const auto addrStr = orthia::AddressToString((*refsPtr)[i].address, dianaMode);
                xrefText += addrStr;
                builder.AddNextRange(addrStr.size(), colors.xref, oui::g_region_id_xref_0 + i);

                if (i + 1 >= refsPtr->size())
                {
                    break;
                }
                ++i;
                if (i >= itemsToWrite)
                {
                    break;
                }
                const PlatformString_type separator = OUI_TCSTR(", ");
                xrefText += separator;
                builder.AddNextRange(separator.size(), colors.bytes);
            }

            if (refsPtr->size() > itemsToWrite)
            {
                const PlatformString_type spaces = OUI_TCSTR(" ");
                xrefText += spaces;
                builder.AddNextRange(spaces.size(), colors.bytes);

                const PlatformString_type bracket = OUI_TCSTR("[...]");
                xrefText += bracket;
                builder.AddNextRange(bracket.size(), colors.xref, oui::g_region_id_xref_dialog);
            }

            const PlatformString_type suffix = OUI_TCSTR(" --");
            xrefText += suffix;
            builder.AddNextRange(suffix.size(), colors.bytes);

            MarkupLine line;
            line.text.native = xrefText;
            line.markup = builder.Build();
            line.hasMarkup = true;
            line.flags |= MarkupLine::flags_HasXRefs;
            allLines.push_back(std::move(line));
        }
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

    // CFilePersistentItemStorage
    CFilePersistentItemStorage::CFilePersistentItemStorage()
    {
    }
    void CFilePersistentItemStorage::Init(orthia::intrusive_ptr<CDatabaseManager> databaseManager)
    {
        m_databaseManager = databaseManager;
        m_databaseManager->GetClassicDatabase()->QueryAllComments([=](Address_type address, const std::string& text) {
            CPersistentItemStorage::SyncWriteComment(address, orthia::Utf8ToPlatformString(text));
            return true;
        });
    }
    oui::fsui::OpenResult CFilePersistentItemStorage::SyncWriteComment(orthia::Address_type address, const oui::String& comment)
    {
        oui::fsui::OpenResult result;
        try
        {
            CPersistentItemStorage::SyncWriteComment(address, comment);
            m_databaseManager->GetClassicDatabase()->InsertComment(address, orthia::PlatformStringToUtf8(comment.native));
        }
        catch (std::exception& e)
        {
            result.error = orthia::Utf8ToPlatformString(e.what());
        }
        return result;
    }
}