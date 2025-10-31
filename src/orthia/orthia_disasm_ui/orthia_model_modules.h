#pragma once

#include "orthia_model_interfaces.h"
#include "diana_pe_cpp.h"
#include "oui_filesystem.h"
#include "orthia_model.h"


namespace orthia
{
    class CImportsLoader :public diana::CBasePeLinkImportsObserver
    {
        struct ModuleInfo
        {
            std::shared_ptr<orthia::CSimplePeFile> peFile;
            bool originalFile = false;
            oui::String fullName;
        };
        std::unordered_map<decltype(oui::String::native), ModuleInfo> m_mappedModules;

        OPERAND_SIZE m_freeSpaceStart = 0;
        int m_dianaMode = 0;

        using ModuleIterator = decltype(m_mappedModules)::iterator;
        std::shared_ptr<oui::IFileSystem> m_pFs;

        std::shared_ptr<oui::BaseOperation> m_operation;

        oui::String NormalizeName(const std::string& dllName);
        oui::String NormalizeName(const oui::String& str);

        oui::String LocateFile(const oui::String& dllName);
        ModuleIterator LoadModule(const std::string& dllName);
        void RelocateModule(std::shared_ptr<orthia::CSimplePeFile> peFile);

        void QueryFunctionImpl(const char* pDllName,
            const char* pFunctionName,
            OPERAND_SIZE ordinal,
            OPERAND_SIZE* pAddress);
        bool CheckConflicts(std::shared_ptr<orthia::CSimplePeFile> peFile);
        void CheckCancel();

    public:
        CImportsLoader(std::shared_ptr<oui::BaseOperation> operation = nullptr);
        void QueryFunctionByOrdinal(const char* pDllName,
            DI_UINT32 ordinal,
            OPERAND_SIZE* pAddress) override;

        void QueryFunctionByName(const char* pDllName,
            const char* pFunctionName,
            DI_UINT32 hint,
            OPERAND_SIZE* pAddress) override;

        void LoadModules(const oui::String& fileName, 
            std::shared_ptr<orthia::CSimplePeFile> peFile,
            std::shared_ptr<oui::IFileSystem> pFs);

        void ReportModules(std::shared_ptr<CModuleManager> moduleManager);
    };
}