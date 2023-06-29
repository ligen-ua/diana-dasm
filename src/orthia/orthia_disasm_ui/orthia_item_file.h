#pragma once

#include "orthia_model_interfaces.h"


namespace orthia
{
    struct FileWorkplaceItem :std::enable_shared_from_this<FileWorkplaceItem>, IWorkPlaceItem
    {
        std::unique_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;
        Address_type moduleLastValidAddress = 0;

        // public interface
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void ReloadModules() override;
    };

}