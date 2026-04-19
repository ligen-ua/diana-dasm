#include "orthia_model_modules_elf.h"
#include "orthia_database_saver.h"
#include "orthia_item_file.h"
#include "orthia_log.h"

namespace orthia
{
    CElfImportsLoader::CElfImportsLoader(std::shared_ptr<oui::BaseOperation> operation)
        :
        m_operation(operation)
    {
    }
    void CElfImportsLoader::CheckCancel()
    {
        if (m_operation && m_operation->IsCancelled())
        {
            throw std::runtime_error("Cancelled");
        }
    }
    void CElfImportsLoader::LoadExports(ModuleInfo& mod)
    {
        if (!mod.elfFile->GetImpl())
            return;

        struct ExportsCollector : public diana::CBasePeLinkImportsObserver
        {
            ModuleInfo& m_mod;
            explicit ExportsCollector(ModuleInfo& m) : m_mod(m) {}
            void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*) override {}
            void QueryFunctionByName(const char*, const char* pFunctionName,
                DI_UINT32, OPERAND_SIZE* pAddress) override
            {
                if (!pFunctionName || !*pFunctionName || !pAddress || !*pAddress)
                    return;
                orthia::NameInfo info;
                info.flags   = orthia::NameInfo::flags_Export;
                info.address = *pAddress;
                info.name    = orthia::Utf8ToPlatformString(pFunctionName);
                m_mod.names.insert({ *pAddress, info });
            }
        } collector(mod);

        int err = mod.elfFile->QueryExports(&collector);
        if (err && err != DI_END)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "QueryExports failed: ", err);
        }

        DI_UINT64 ep = mod.elfFile->GetEntryPoint();
        if (ep)
        {
            orthia::NameInfo epInfo;
            epInfo.flags   = orthia::NameInfo::flags_Export;
            epInfo.address = ep;
            epInfo.name    = orthia::Utf8ToPlatformString("$entrypoint");
            mod.names.insert({ ep, epInfo });
        }
    }
    void CElfImportsLoader::LoadImports(ModuleInfo& mod)
    {
        if (!mod.elfFile->GetImpl())
            return;

        struct ImportsCollector : public diana::CBasePeLinkImportsObserver
        {
            ModuleInfo& m_mod;
            explicit ImportsCollector(ModuleInfo& m) : m_mod(m) {}
            void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*) override {}
            void QueryFunctionByName(const char* pDllName, const char* pFunctionName,
                DI_UINT32, OPERAND_SIZE* pAddress) override
            {
                if (!pFunctionName || !*pFunctionName)
                    return;
                std::string nameStr;
                if (pDllName && *pDllName)
                    nameStr = std::string(pDllName) + "!" + pFunctionName;
                else
                    nameStr = pFunctionName;

                orthia::NameInfo info;
                info.flags   = orthia::NameInfo::flags_Import;
                info.address = pAddress ? *pAddress : 0;
                info.name    = orthia::Utf8ToPlatformString(nameStr);
                m_mod.names.insert({ GetLastAddress(), info });
            }
        } collector(mod);

        int err = mod.elfFile->QueryImports(&collector);
        if (err && err != DI_END)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "QueryImports failed: ", err);
        }
    }

    void CElfImportsLoader::LoadModules(
        const oui::String& fileName,
        std::shared_ptr<orthia::CSimpleElfFile> elfFile,
        std::shared_ptr<oui::IFileSystem> /*pFs*/)
    {
        CheckCancel();
        m_dianaMode = elfFile->GetDianaMode();

        oui::String shortFileName;
        orthia::UnparseFileNameFromFullFileName(fileName.native, &shortFileName.native);

        ModuleInfo info;
        info.elfFile      = elfFile;
        info.originalFile = true;
        info.fullName     = fileName;
        auto it = m_mappedModules.insert({ shortFileName.native, info }).first;

        try
        {
            LoadExports(it->second);
        }
        catch (std::exception& e)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "Can't load ELF exports: ", e.what());
        }
        try
        {
            LoadImports(it->second);
        }
        catch (std::exception& e)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "Can't load ELF imports: ", e.what());
        }
    }

    void CElfImportsLoader::InsertNames(std::shared_ptr<CModuleManager> moduleManager,
        const ModuleInfo& mod)
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        for (auto& name : mod.names)
        {
            InsertName(classicDatabase, mod.elfFile->GetImageBase(), name.second, name.first);
        }
    }

    void CElfImportsLoader::ReportModules(std::shared_ptr<CModuleManager> moduleManager)
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        CClassicDatabaseModuleCleaner cleaner(classicDatabase.get());

        for (auto& mod : m_mappedModules)
        {
            if (mod.second.originalFile)
            {
                InsertNames(moduleManager, mod.second);
                continue;
            }
            oui::String shortName;
            orthia::UnparseFileNameFromFullFileName(mod.second.fullName.native, &shortName.native);

            CAutoRollbackClassicDatabase rollback;
            classicDatabase->StartSaveModule(
                mod.second.elfFile->GetImageBase(),
                mod.second.elfFile->GetMappedFile().size(),
                shortName.native,
                &rollback);

            InsertModuleMetaInfo(classicDatabase,
                mod.second.elfFile->GetImageBase(),
                mod.second.fullName.native);

            InsertNames(moduleManager, mod.second);
            classicDatabase->DoneSave();
        }
    }
}
