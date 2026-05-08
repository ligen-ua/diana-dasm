#include "orthia_model_modules_elf.h"
#include "orthia_database_saver.h"
#include "orthia_item_file.h"
#include "orthia_log.h"

namespace orthia
{
    static OPERAND_SIZE RoundUp(OPERAND_SIZE value, OPERAND_SIZE alignment)
    {
        if ((value % alignment) == 0)
            return value;
        return value - (value % alignment) + alignment;
    }

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

    oui::String CElfImportsLoader::NormalizeName(const std::string& libName)
    {
        return oui::String(orthia::Utf8ToPlatformString(libName));
    }
    oui::String CElfImportsLoader::LocateFile(const oui::String& libName)
    {
        int platformError = 0;
        oui::String fullName;
        std::tie(platformError, fullName) = m_pFs->SyncLocateFile(libName, m_dianaMode);
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't locate: " + orthia::PlatformStringToUtf8(libName.native), platformError);
        }
        return fullName;
    }
    OPERAND_SIZE CElfImportsLoader::GetLastPossibleAddress()
    {
        OPERAND_SIZE lastPossibleAddress = DI_MAX_OPERAND_SIZE;
        switch (m_dianaMode)
        {
        case 4:
            lastPossibleAddress = std::numeric_limits<uint32_t>::max();
            break;
        case 2:
            lastPossibleAddress = std::numeric_limits<uint16_t>::max();
            break;
        }
        return lastPossibleAddress;
    }
    bool CElfImportsLoader::CheckConflicts(std::shared_ptr<CSimpleElfFile> elfFile)
    {
        auto modAddress = elfFile->GetImageBase();
        auto modEnd = elfFile->GetImageEnd();
        if (!modAddress)
            return true;
        for (auto& mod : m_mappedModules)
        {
            if (!mod.second.elfFile->GetImpl())
                continue;
            auto curAddress = mod.second.elfFile->GetImageBase();
            auto curEnd = mod.second.elfFile->GetImageEnd();
            if (curAddress > modAddress && curAddress < modEnd) return true;
            if (curEnd > modAddress && curEnd < modEnd) return true;
            if (curAddress <= modAddress && curEnd >= modEnd) return true;
            if (modAddress <= curAddress && modEnd >= curEnd) return true;
        }
        return false;
    }
    void CElfImportsLoader::RelocateModule(std::shared_ptr<CSimpleElfFile> elfFile)
    {
        OPERAND_SIZE lastPossibleAddress = GetLastPossibleAddress();
        if (m_freeSpaceStart > lastPossibleAddress)
            throw std::runtime_error("Can't load module: address space exhausted");
        auto possibleAddress = RoundUp(m_freeSpaceStart, 0x10000);
        elfFile->Relocate(possibleAddress);
    }
    CElfImportsLoader::ModuleIterator CElfImportsLoader::LoadModule(const std::string& libName)
    {
        try
        {
            return LoadModuleImpl(libName);
        }
        catch (std::exception& e)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "Can't load ELF dep ", libName, ": ", e.what());
            oui::String name;
            try { name = NormalizeName(libName); }
            catch (...) { name = orthia::Utf8ToPlatformString(libName); }

            auto it = m_mappedModules.find(name.native);
            if (it != m_mappedModules.end())
                return it;
            ModuleInfo info;
            info.fullName = name;
            info.elfFile = std::make_shared<CSimpleElfFile>();
            return m_mappedModules.insert({ name.native, info }).first;
        }
    }
    CElfImportsLoader::ModuleIterator CElfImportsLoader::LoadModuleImpl(const std::string& libName)
    {
        auto normalName = NormalizeName(libName);
        {
            auto it = m_mappedModules.find(normalName.native);
            if (it != m_mappedModules.end())
                return it;
        }

        auto fullName = LocateFile(normalName);

        int platformError = 0;
        std::shared_ptr<oui::IFile2> file;
        std::tie(platformError, file) = m_pFs->SyncOpenFile(oui::FileUnifiedId(fullName));
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't open: " + orthia::PlatformStringToUtf8(fullName.native), platformError);
        }

        std::vector<char> rawFile;
        oui::String error = ReadFileToVector(file, rawFile);
        if (!error.native.empty())
        {
            throw orthia::CWin32Exception("Can't read: " + orthia::PlatformStringToUtf8(fullName.native), platformError);
        }

        auto mappedElf = std::make_shared<CSimpleElfFile>();
        orthia::MapFileParameters params;
        params.mapFlags = DIANA_ELF_MAP_DO_NOT_RELOCATE;
        mappedElf->MapFile(rawFile, params);

        if (mappedElf->GetDianaMode() != m_dianaMode)
        {
            throw std::runtime_error("Bitness mismatch: " + orthia::PlatformStringToUtf8(fullName.native));
        }

        if (CheckConflicts(mappedElf))
            RelocateModule(mappedElf);
        else
            mappedElf->Relocate(mappedElf->GetImageBase());

        if (m_freeSpaceStart < mappedElf->GetImageEnd())
            m_freeSpaceStart = mappedElf->GetImageEnd();

        OPERAND_SIZE lastPossibleAddress = GetLastPossibleAddress();
        if (m_freeSpaceStart > lastPossibleAddress)
            throw std::runtime_error("Can't load module");

        ModuleInfo info;
        info.elfFile = mappedElf;
        info.fullName = fullName;
        return m_mappedModules.insert({ normalName.native, info }).first;
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
    void CElfImportsLoader::LinkImports(ModuleInfo& mod)
    {
        if (!mod.elfFile->GetImpl())
            return;

        struct Resolver : public diana::CBasePeLinkImportsObserver
        {
            CElfImportsLoader& loader;
            explicit Resolver(CElfImportsLoader& l) : loader(l) {}
            void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*) override {}
            void QueryFunctionByName(const char*, const char* pFunctionName,
                DI_UINT32, OPERAND_SIZE* pAddress) override
            {
                if (!pFunctionName || !*pFunctionName || !pAddress)
                    return;
                for (auto& m : loader.m_mappedModules)
                {
                    if (!m.second.elfFile->GetImpl())
                        continue;
                    auto addr = m.second.elfFile->DiGetProcAddress(pFunctionName);
                    if (addr)
                    {
                        *pAddress = addr;
                        return;
                    }
                }
            }
        } resolver(*this);

        mod.elfFile->QueryImports(&resolver);
    }

    void CElfImportsLoader::LoadModules(
        const oui::String& fileName,
        std::shared_ptr<orthia::CSimpleElfFile> elfFile,
        std::shared_ptr<oui::IFileSystem> pFs)
    {
        CheckCancel();
        m_pFs = pFs;
        m_dianaMode = elfFile->GetDianaMode();
        m_freeSpaceStart = elfFile->GetImageEnd();

        oui::String shortFileName;
        orthia::UnparseFileNameFromFullFileName(fileName.native, &shortFileName.native);

        ModuleInfo rootInfo;
        rootInfo.elfFile      = elfFile;
        rootInfo.originalFile = true;
        rootInfo.fullName     = fileName;
        auto rootKey = shortFileName.native;
        m_mappedModules.insert({ rootKey, rootInfo });

        // BFS over DT_NEEDED chain; store keys (not iterators) since insertions
        // may rehash the unordered_map and invalidate iterators
        std::vector<decltype(rootKey)> toProcess = { rootKey };
        for (size_t i = 0; i < toProcess.size(); ++i)
        {
            CheckCancel();
            auto curIt = m_mappedModules.find(toProcess[i]);
            if (curIt == m_mappedModules.end() || !curIt->second.elfFile->GetImpl())
                continue;

            std::vector<std::string> needed;
            try { needed = curIt->second.elfFile->GetNeededLibraries(); }
            catch (std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "Can't get ELF needed libs: ", e.what());
                continue;
            }

            for (auto& libName : needed)
            {
                auto sizeBefore = m_mappedModules.size();
                auto modIt = LoadModule(libName);
                // Only enqueue if this was a new entry
                if (m_mappedModules.size() > sizeBefore)
                    toProcess.push_back(modIt->first);
            }
        }

        // Load exports for all modules (including dependencies, for name resolution)
        for (auto& mod : m_mappedModules)
        {
            try { LoadExports(mod.second); }
            catch (std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "Can't load ELF exports: ", e.what());
            }
        }

        // Link root's imports against the fully loaded chain (patches GOT slots)
        auto rootFinal = m_mappedModules.find(rootKey);
        if (rootFinal != m_mappedModules.end())
        {
            try { LinkImports(rootFinal->second); }
            catch (std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "Can't link ELF imports: ", e.what());
            }
            // Collect import names (with now-resolved addresses)
            try { LoadImports(rootFinal->second); }
            catch (std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "Can't load ELF imports: ", e.what());
            }
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
            if (!mod.second.elfFile->GetImpl())
                continue;
            oui::String shortName;
            orthia::UnparseFileNameFromFullFileName(mod.second.fullName.native, &shortName.native);

            CAutoRollbackClassicDatabase rollback;
            classicDatabase->StartSaveModule(
                mod.second.elfFile->GetImageBase(),
                mod.second.elfFile->GetMappedFile().size(),
                shortName.native,
                &rollback,
                true);

            InsertModuleMetaInfo(classicDatabase,
                mod.second.elfFile->GetImageBase(),
                mod.second.fullName.native,
                0,
                orthia::ModuleInfo::builtInFlags_moduleTypeElf);

            InsertNames(moduleManager, mod.second);
            classicDatabase->DoneSave();
        }
    }
}
