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
        Address_type m_processModuleAddress = 0;
    public:
        CProcessWorkplaceItem(std::shared_ptr<oui::IProcess> proc,
            const oui::String& shortName,
            int dianaMode);

        void ReloadModules() override;
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;

        Address_type GerProcessModuleAddress();

    };

}