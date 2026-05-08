#include "orthia_module_symbols.h"
#include "orthia_database_module.h"
#include "orthia_item_file.h"
#include "orthia_common_format.h"

namespace orthia
{
    // ModuleSymbols

    void ModuleSymbols::Insert(Address_type address, NameInfo info, int priority)
    {
        auto it = m_pending.find(address);
        if (it == m_pending.end() || priority > it->second.second)
            m_pending[address] = { std::move(info), priority };
    }

    void ModuleSymbols::Finalize()
    {
        for (auto& [addr, pair] : m_pending)
            m_data.insert(addr, pair.first);
        m_data.sort();
        std::unordered_map<Address_type, std::pair<NameInfo, int>>().swap(m_pending);
    }

    bool ModuleSymbols::IsEmpty() const
    {
        return m_data.begin() == m_data.end();
    }

    void ModuleSymbols::ForEach(std::function<void(Address_type, const NameInfo&)> fn) const
    {
        for (const auto& [addr, info] : m_data)
            fn(addr, info);
    }

    void ModuleSymbols::FlushToDB(Address_type moduleAddress, intrusive_ptr<CClassicDatabase> db) const
    {
        constexpr int kBatchSize = 1000;
        auto it = m_data.begin();
        auto it_end = m_data.end();
        while (it != it_end)
        {
            auto batch = db->BeginBatch();
            for (int i = 0; i < kBatchSize && it != it_end; ++i, ++it)
                InsertName(db, moduleAddress, it->second, it->first);
            batch->Commit();
        }
    }

    const NameInfo* ModuleSymbols::QueryNearest(Address_type address) const
    {
        auto it = m_data.upper_bound(address);
        if (it == m_data.begin())
            return nullptr;
        --it;
        return &it->second;
    }

    void ModuleSymbols::QueryPrivateSymbols(const NameSelectionKey& filter,
                                             int count,
                                             std::vector<NameInfo>& names,
                                             int* totalCount,
                                             bool& markFound) const
    {
        Address_type addressHint = markFound ? 0 : filter.address;
        auto it = m_data.lower_bound(addressHint);
        for (; it != m_data.end(); ++it)
        {
            if (totalCount)
            {
                ++(*totalCount);
                continue;
            }
            if (!markFound)
            {
                if (it->first == filter.address)
                    markFound = true;
                continue;
            }
            NameInfo info;
            info.address = it->second.address;
            info.name = it->second.name;
            info.flags = NameInfo::flags_PrivateSymbol;
            names.push_back(std::move(info));
            if (count && (int)names.size() >= count)
                return;
        }
    }

    // ModuleStorage

    ModuleStorage::ModuleStorage(intrusive_ptr<CClassicDatabase> db)
        : m_db(std::move(db))
    {
    }

    void ModuleStorage::Store(Address_type moduleAddress, ModuleSymbols symbols)
    {
        CAutoCriticalSection guard(m_lock);
        m_modules[moduleAddress] = std::move(symbols);
    }

    void ModuleStorage::QueryNearestPrivateSymbol(Address_type address,
                                                   NameInfo& nameInfo) const
    {
        Address_type bestAddress = 0;
        NameInfo bestInfo;
        bool found = false;
        {
            CAutoCriticalSection guard(m_lock);
            for (const auto& [modAddr, syms] : m_modules)
            {
                if (const NameInfo* sym = syms.QueryNearest(address))
                {
                    if (sym->address >= bestAddress)
                    {
                        bestAddress = sym->address;
                        bestInfo = *sym;
                        found = true;
                    }
                }
            }
        }
        if (found)
        {
            if (bestAddress == address)
            {
                nameInfo.flags |= NameInfo::flags_PrivateSymbol;
                nameInfo.privateSymbol = bestInfo.name;
            }
            else
            {
                nameInfo.privateSymbol = ComposeName(bestInfo.name, bestAddress, address);
            }
            return;
        }

        if (!m_db)
            return;

        m_db->QueryMetaInfoByNearestAddress(
            g_database_type_fnc_PrivateSymbol,
            address,
            [&](Address_type, int, const std::string& text, Address_type) -> bool {
                std::string nameStr;
                Address_type target = 0;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("address", &target);
                parser.QueryMetadata("name", &nameStr);
                if (target == address)
                {
                    nameInfo.flags |= NameInfo::flags_PrivateSymbol;
                    nameInfo.privateSymbol = Utf8ToPlatformString(nameStr);
                }
                else
                {
                    nameInfo.privateSymbol = ComposeName(Utf8ToPlatformString(nameStr), target, address);
                }
                return false;
            });
    }

    void ModuleStorage::QueryModulePrivateSymbols(Address_type moduleAddress,
                                                   const NameSelectionKey& filter,
                                                   int count,
                                                   std::vector<NameInfo>& names,
                                                   int* totalCount,
                                                   bool& markFound) const
    {
        {
            CAutoCriticalSection guard(m_lock);
            auto it = m_modules.find(moduleAddress);
            if (it != m_modules.end())
            {
                it->second.QueryPrivateSymbols(filter, count, names, totalCount, markFound);
                return;
            }
        }

        if (!m_db)
            return;

        Address_type addressHint = markFound ? 0 : filter.address;
        m_db->QueryMetaInfoModule2(moduleAddress,
            g_database_type_fnc_PrivateSymbol, -1,
            [&, markFound](Address_type, int, const std::string& text, Address_type) mutable -> bool {
                if (totalCount)
                {
                    ++(*totalCount);
                    return true;
                }
                std::string nameStr;
                Address_type target = 0;
                CCommonFormatParser parser;
                parser.Parse(text);
                parser.QueryMetadata("address", &target);
                parser.QueryMetadata("name", &nameStr);
                if (!markFound)
                {
                    if (target == filter.address)
                        markFound = true;
                    return true;
                }
                NameInfo info;
                info.name = Utf8ToPlatformString(nameStr);
                info.address = target;
                info.flags = NameInfo::flags_PrivateSymbol;
                names.push_back(std::move(info));
                return !count || (int)names.size() < count;
            }, addressHint);
    }
}
