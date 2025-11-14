#pragma once

#include "orthia_model_interfaces.h"

namespace orthia
{
    struct FileWorkplaceItem :std::enable_shared_from_this<FileWorkplaceItem>, IWorkPlaceItem
    {

        std::shared_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;
        Address_type moduleLastValidAddress = 0;
        std::shared_ptr<IPeristentItemStorage> persistentItemStorage;

        FileWorkplaceItem(std::shared_ptr<IPeristentItemStorage> peristentItemStorage_in) 
            : persistentItemStorage(peristentItemStorage_in)
        {
        }

        // public interface
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void ReloadModules() override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;
        std::shared_ptr<IPeristentItemStorage> GetPersistentStorage() override;
        int GetDianaMode() const override;
        void QueryNames(Address_type moduleAddress, const NameSelectionKey& name, int count, std::vector<NameInfo>& names) const override;
        int QueryNamesCount(Address_type moduleAddress, const NameSelectionKey& name) const override;

        int GetModulesEx(bool calcCount, std::vector<orthia::ModuleInfo>& modules) const;
    };

    class CClassicDatabase;
    void InsertModuleMetaInfo(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const oui::String & fullName);
    void InsertName(orthia::intrusive_ptr<CClassicDatabase> database, Address_type moduleAddress, const orthia::NameInfo& info);
}