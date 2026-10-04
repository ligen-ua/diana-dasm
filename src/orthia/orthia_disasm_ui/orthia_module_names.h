#pragma once

#include "orthia_model_interfaces.h"
#include <list>
#include <map>
#include <memory>
#include <functional>

namespace orthia
{
    // All names of one module: exports, imports, private symbols.
    // Paged by offset in any NameSortOrder, the sorted views are built on demand.
    class ModuleNames
    {
        std::vector<NameInfo> m_names;   // exports, imports, private symbols; each group by address
        mutable CCriticalSection m_lock;
        mutable std::map<int, std::vector<int>> m_sortedIndexes;

        const std::vector<int>& QueryIndex(const NameSelectionKey& key) const;
    public:
        void Add(NameInfo info);
        void Finalize();
        int QueryCount(const NameSelectionKey& key) const;
        void QueryPage(const NameSelectionKey& key, int count, std::vector<NameInfo>& names) const;
    };

    // Caches the names of the last few modules queried by an item.
    class ModuleNamesStorage
    {
        static const int c_maxModules = 4;

        mutable CCriticalSection m_lock;
        std::list<std::pair<Address_type, std::shared_ptr<const ModuleNames>>> m_modules; // most recent first
        long long m_generation = 0; // bumped by Invalidate/Clear, so a build racing with them is not cached
    public:
        std::shared_ptr<const ModuleNames> Query(Address_type moduleAddress,
                                                 std::function<void(ModuleNames&)> build);
        void Invalidate(Address_type moduleAddress);
        void Clear();
    };
}
