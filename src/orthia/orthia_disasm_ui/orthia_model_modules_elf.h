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
        OPERAND_SIZE m_freeSpaceStart = 0;
        int m_dianaMode = 0;
        std::shared_ptr<oui::IFileSystem> m_pFs;
        std::shared_ptr<oui::BaseOperation> m_operation;

        using ModuleIterator = decltype(m_mappedModules)::iterator;

        oui::String NormalizeName(const std::string& libName);
        oui::String LocateFile(const oui::String& libName);
        bool CheckConflicts(std::shared_ptr<CSimpleElfFile> elfFile);
        void RelocateModule(std::shared_ptr<CSimpleElfFile> elfFile);
        OPERAND_SIZE GetLastPossibleAddress();
        ModuleIterator LoadModule(const std::string& libName);
        ModuleIterator LoadModuleImpl(const std::string& libName);

        void CheckCancel();
        void LoadExports(ModuleInfo& mod);
        void LoadImports(ModuleInfo& mod);
        void LinkImports(ModuleInfo& mod);
        void InsertNames(std::shared_ptr<CModuleManager> moduleManager, const ModuleInfo& mod);

    public:
        explicit CElfImportsLoader(std::shared_ptr<oui::BaseOperation> operation = nullptr);

        void LoadModules(const oui::String& fileName,
            std::shared_ptr<orthia::CSimpleElfFile> elfFile,
            std::shared_ptr<oui::IFileSystem> pFs);

        void ReportModules(std::shared_ptr<CModuleManager> moduleManager);
    };
}
