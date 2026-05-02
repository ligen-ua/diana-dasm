#pragma once

#include "orthia_model_interfaces.h"
#include <memory>
#include <vector>

namespace orthia
{
    class CClassicDatabase;

    class IExternalSymbolsLoader
    {
    public:
        virtual ~IExternalSymbolsLoader() = default;
        virtual bool CanLoad(const ModuleInfo& mod) const = 0;
        virtual void Load(const ModuleInfo& mod,
                          intrusive_ptr<CClassicDatabase> db) = 0;
    };

    std::unique_ptr<IExternalSymbolsLoader> CreateExternalSymbolsLoader(
        const std::vector<PlatformString_type>& symbolFolders);
}
