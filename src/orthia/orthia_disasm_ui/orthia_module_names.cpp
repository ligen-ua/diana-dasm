#include "orthia_module_names.h"
#include <algorithm>

namespace orthia
{
    static int NameTypeRank(const NameInfo& info)
    {
        if (info.flags & NameInfo::flags_Export)
            return 0;
        if (info.flags & NameInfo::flags_Import)
            return 1;
        if (info.flags & NameInfo::flags_PrivateSymbol)
            return 2;
        return 3;
    }

    static const int c_filterAll = 0;
    static const int c_filterExcludeImports = 1;
    static const int c_filterPrivateSymbolsOnly = 2;

    static int QueryFilter(const NameSelectionKey& key)
    {
        if (key.privateSymbolsOnly)
            return c_filterPrivateSymbolsOnly;
        if (key.excludeImports)
            return c_filterExcludeImports;
        return c_filterAll;
    }

    static bool PassesFilter(int filter, const NameInfo& info)
    {
        switch (filter)
        {
        case c_filterExcludeImports:
            return !(info.flags & NameInfo::flags_Import);
        case c_filterPrivateSymbolsOnly:
            return (info.flags & NameInfo::flags_PrivateSymbol) != 0;
        default:
            return true;
        }
    }

    // ModuleNames

    void ModuleNames::Add(NameInfo info)
    {
        m_names.push_back(std::move(info));
    }

    void ModuleNames::Finalize()
    {
        std::stable_sort(m_names.begin(), m_names.end(), [](const NameInfo& left, const NameInfo& right) {
            int leftRank = NameTypeRank(left), rightRank = NameTypeRank(right);
            if (leftRank != rightRank)
                return leftRank < rightRank;
            return left.address < right.address;
        });
        CAutoCriticalSection guard(m_lock);
        m_sortedIndexes.clear();
    }

    const std::vector<int>& ModuleNames::QueryIndex(const NameSelectionKey& key) const
    {
        const int filter = QueryFilter(key);
        const int sortOrder = (int)key.sortOrder;

        CAutoCriticalSection guard(m_lock);
        // std::map nodes are stable: the index stays valid after the lock is released
        auto it = m_sortedIndexes.find(filter * 3 + sortOrder);
        if (it != m_sortedIndexes.end())
            return it->second;

        std::vector<int> index;
        index.reserve(m_names.size());
        for (int i = 0, size = (int)m_names.size(); i < size; ++i)
        {
            if (PassesFilter(filter, m_names[i]))
                index.push_back(i);
        }

        if (key.sortOrder != NameSortOrder::Type)
        {
            std::vector<PlatformString_type> keys(m_names.size());
            for (int i : index)
                keys[i] = Downcase(m_names[i].name.native);

            if (key.sortOrder == NameSortOrder::Name)
            {
                // the type order is the last tie-break: index is already in it
                std::stable_sort(index.begin(), index.end(), [&](int left, int right) {
                    if (keys[left] != keys[right])
                        return keys[left] < keys[right];
                    return m_names[left].address < m_names[right].address;
                });
            }
            else
            {
                std::stable_sort(index.begin(), index.end(), [&](int left, int right) {
                    if (m_names[left].address != m_names[right].address)
                        return m_names[left].address < m_names[right].address;
                    int leftRank = NameTypeRank(m_names[left]), rightRank = NameTypeRank(m_names[right]);
                    if (leftRank != rightRank)
                        return leftRank < rightRank;
                    return keys[left] < keys[right];
                });
            }
        }
        return m_sortedIndexes.emplace(filter * 3 + sortOrder, std::move(index)).first->second;
    }

    int ModuleNames::QueryCount(const NameSelectionKey& key) const
    {
        return (int)QueryIndex(key).size();
    }

    void ModuleNames::QueryPage(const NameSelectionKey& key, int count, std::vector<NameInfo>& names) const
    {
        names.clear();
        if (count <= 0 || key.offset < 0)
            return;
        const auto& index = QueryIndex(key);
        size_t first = (size_t)key.offset;
        if (first >= index.size())
            return;
        size_t last = std::min(index.size(), first + (size_t)count);
        names.reserve(last - first);
        for (size_t i = first; i < last; ++i)
            names.push_back(m_names[index[i]]);
    }

    // ModuleNamesStorage

    std::shared_ptr<const ModuleNames> ModuleNamesStorage::Query(Address_type moduleAddress,
                                                                 std::function<void(ModuleNames&)> build)
    {
        long long generation = 0;
        {
            CAutoCriticalSection guard(m_lock);
            for (auto it = m_modules.begin(); it != m_modules.end(); ++it)
            {
                if (it->first == moduleAddress)
                {
                    m_modules.splice(m_modules.begin(), m_modules, it);
                    return m_modules.front().second;
                }
            }
            generation = m_generation;
        }

        // built outside the lock: a module can take a while to read
        auto names = std::make_shared<ModuleNames>();
        build(*names);
        names->Finalize();

        CAutoCriticalSection guard(m_lock);
        // invalidated while building: the result may already be stale, do not keep it
        if (generation != m_generation)
            return names;
        m_modules.remove_if([&](const auto& entry) { return entry.first == moduleAddress; });
        m_modules.emplace_front(moduleAddress, names);
        if ((int)m_modules.size() > c_maxModules)
            m_modules.pop_back();
        return names;
    }

    void ModuleNamesStorage::Invalidate(Address_type moduleAddress)
    {
        CAutoCriticalSection guard(m_lock);
        ++m_generation;
        m_modules.remove_if([&](const auto& entry) { return entry.first == moduleAddress; });
    }

    void ModuleNamesStorage::Clear()
    {
        CAutoCriticalSection guard(m_lock);
        ++m_generation;
        m_modules.clear();
    }
}
