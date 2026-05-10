#pragma once

#include "orthia_model_interfaces.h"
#include <unordered_map>
#include <map>
#include <functional>

namespace orthia
{
    class CClassicDatabase;

    class ModuleSymbols
    {
        std::unordered_map<Address_type, std::pair<NameInfo, int>> m_pending;
        flat_map<Address_type, NameInfo> m_data;
    public:
        void Insert(Address_type address, NameInfo info, int priority);
        void Finalize();
        bool IsEmpty() const;
        void ForEach(std::function<void(Address_type, const NameInfo&)> fn) const;
        void FlushToDB(Address_type moduleAddress, intrusive_ptr<CClassicDatabase> db) const;
        const NameInfo* QueryNearest(Address_type address) const;
        void QueryPrivateSymbols(const NameSelectionKey& filter,
                                 int count,
                                 std::vector<NameInfo>& names,
                                 int* totalCount,
                                 bool& markFound) const;
    };

    // Owns both storage layers for a process item's private symbols.
    // In-memory (ModuleSymbols) takes priority; DB is the filesystem fallback.
    class ModuleStorage
    {
        mutable CCriticalSection m_lock;
        std::map<Address_type, ModuleSymbols> m_modules;
        mutable intrusive_ptr<CClassicDatabase> m_db;
    public:
        explicit ModuleStorage(intrusive_ptr<CClassicDatabase> db);

        void Store(Address_type moduleAddress, ModuleSymbols symbols);

        // In-memory if the module was loaded this session; DB otherwise.
        void QueryNearestPrivateSymbol(Address_type address,
                                       NameInfo& nameInfo) const;

        // In-memory if the module was loaded this session; DB otherwise.
        void QueryModulePrivateSymbols(Address_type moduleAddress,
                                       const NameSelectionKey& filter,
                                       int count,
                                       std::vector<NameInfo>& names,
                                       int* totalCount,
                                       bool& markFound) const;
    };
}
