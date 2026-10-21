#pragma once

#include "orthia_model_interfaces.h"
#include "diana_pe_cpp.h"
#include "oui_filesystem.h"
#include "orthia_elf.h"
#include "orthia_model.h"
#include "orthia_image_identity.h"
#include "formats/orthia_image_format.h"

namespace orthia
{
    class CElfImportsLoader : public IDependencyLoader
    {
        struct ModuleInfo
        {
            std::shared_ptr<orthia::CSimpleElfFile> elfFile;
            bool originalFile = false;
            oui::String fullName;
            std::unordered_map<Address_type, orthia::NameInfo> names;
            // the range the module occupies: an unresolved dependency has no image, but still
            // gets a range of its own, so every module keeps a unique address
            Address_type base = 0;
            Address_type size = 0;
            bool unresolved = false;
            ImageIdentity identity;
        };
        std::unordered_map<decltype(oui::String::native), ModuleInfo> m_mappedModules;
        OPERAND_SIZE m_freeSpaceStart = 0;
        int m_dianaMode = 0;
        int m_dianaArch = 0;   // a library of another CPU (the host's x86-64 libc for an AArch64 image) is not linked
        oui::String m_exeDirectory;   // searched first, like the dynamic loader's rpath of the binary
        std::shared_ptr<oui::IFileSystem> m_pFs;
        std::shared_ptr<oui::BaseOperation> m_operation;

        using ModuleIterator = decltype(m_mappedModules)::iterator;

        oui::String NormalizeName(const std::string& libName);
        oui::String LocateFile(const oui::String& libName);
        bool CheckConflicts(std::shared_ptr<CSimpleElfFile> elfFile);
        void RelocateModule(std::shared_ptr<CSimpleElfFile> elfFile);
        OPERAND_SIZE GetLastPossibleAddress();
        Address_type ReserveSpace(OPERAND_SIZE size);
        ModuleIterator LoadModule(const std::string& libName);
        ModuleIterator LoadModuleImpl(const std::string& libName);
        ModuleIterator InsertUnresolved(const std::string& libName);

        void CheckCancel();
        void LoadExports(ModuleInfo& mod);
        void LinkImports(ModuleInfo& mod);
        void InsertNames(std::shared_ptr<CModuleManager> moduleManager, const ModuleInfo& mod);

    public:
        explicit CElfImportsLoader(std::shared_ptr<oui::BaseOperation> operation = nullptr);

        // IDependencyLoader; file is a CSimpleElfFile
        void LoadModules(const oui::String& fileName,
            std::shared_ptr<ISimpleFile> file,
            std::shared_ptr<oui::IFileSystem> pFs) override;
        void ReportModules(std::shared_ptr<CModuleManager> moduleManager) override;
        // the main module's import rows carry the GOT slot address
        bool ImportsKeyedBySlot() const override { return true; }
    };
}
