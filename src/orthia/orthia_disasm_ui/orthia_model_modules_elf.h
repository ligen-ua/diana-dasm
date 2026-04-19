#pragma once

#include "orthia_model_interfaces.h"
#include "diana_pe_cpp.h"
#include "oui_filesystem.h"
#include "orthia_elf.h"
#include "orthia_model.h"

namespace orthia
{
    class CElfImportsLoader
    {
        struct ModuleInfo
        {
            std::shared_ptr<orthia::CSimpleElfFile> elfFile;
            bool originalFile = false;
            oui::String fullName;
            std::unordered_map<Address_type, orthia::NameInfo> names;
        };
        std::unordered_map<decltype(oui::String::native), ModuleInfo> m_mappedModules;
        int m_dianaMode = 0;
        std::shared_ptr<oui::BaseOperation> m_operation;

        void CheckCancel();
        void LoadExports(ModuleInfo& mod);
        void LoadImports(ModuleInfo& mod);
        void InsertNames(std::shared_ptr<CModuleManager> moduleManager, const ModuleInfo& mod);

    public:
        explicit CElfImportsLoader(std::shared_ptr<oui::BaseOperation> operation = nullptr);

        void LoadModules(const oui::String& fileName,
            std::shared_ptr<orthia::CSimpleElfFile> elfFile,
            std::shared_ptr<oui::IFileSystem> pFs);

        void ReportModules(std::shared_ptr<CModuleManager> moduleManager);
    };
}
