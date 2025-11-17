#pragma once

#include "oui_processes.h"


namespace orthia
{
    class CProcessWorkplaceItem:public std::enable_shared_from_this<CProcessWorkplaceItem>, public IWorkPlaceItem
    {
        mutable orthia::CCriticalSection m_lock;
        std::shared_ptr<oui::IProcess> m_proc;
        oui::String m_shortName;
        int m_dianaMode = 0;

        std::vector<orthia::ModuleInfo> m_modules;
        std::map<orthia::Address_type, int> m_modulesIndex;
        std::unordered_map<orthia::Address_type, oui::String> m_exports;

        Address_type m_processModuleAddress = 0;
        std::shared_ptr<IPeristentItemStorage> m_persistentStorage;

        void QueryNamesEx(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names, int* totalCount) const;

    public:
        CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
            const oui::String& shortName,
            int dianaMode,
            std::shared_ptr<IPeristentItemStorage> persistentStorage);

        void ReloadModules() override;
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;
        int GetDianaMode() const override { return m_dianaMode;}
        Address_type GerProcessModuleAddress();
        std::shared_ptr<IPeristentItemStorage> GetPersistentStorage() override;
        void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names) const override;
        int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const override;
        MarkupRangeInfo QueryMarkupRange(Address_type address) const override;
        void QueryMarkupRange(Address_type address, int index, int count, MarkupRange& range) const override;
    };

}