#include "orthia_model_modules_elf.h"
#include "orthia_database_saver.h"
#include "orthia_item_file.h"
#include "orthia_image_source.h"
#include "orthia_helpers.h"
#include "orthia_log.h"
#include "formats/orthia_image_format.h"

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
        if (!m_exeDirectory.native.empty())
        {
            std::tie(platformError, fullName) = m_pFs->SyncLocateFile(oui::String(m_exeDirectory.native + libName.native), m_dianaMode);
            if (!platformError)
            {
                return fullName;
            }
        }
        std::tie(platformError, fullName) = m_pFs->SyncLocateFile(libName, m_dianaMode);
        if (platformError)
        {
            // a library the host has no copy of is expected (an ELF opened on Windows), not an error
            return oui::String();
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
    Address_type CElfImportsLoader::ReserveSpace(OPERAND_SIZE size)
    {
        OPERAND_SIZE lastPossibleAddress = GetLastPossibleAddress();
        auto address = RoundUp(m_freeSpaceStart, 0x10000);
        if (address < m_freeSpaceStart || address > lastPossibleAddress || size > lastPossibleAddress - address)
        {
            throw std::runtime_error("Can't reserve space: address space exhausted");
        }
        m_freeSpaceStart = address + size;
        return address;
    }
    bool CElfImportsLoader::CheckConflicts(std::shared_ptr<CSimpleElfFile> elfFile)
    {
        auto modAddress = elfFile->GetImageBase();
        auto modEnd = elfFile->GetImageEnd();
        if (!modAddress)
            return true;
        for (auto& mod : m_mappedModules)
        {
            // an unresolved dependency has no image, but its reserved range is taken
            Address_type curAddress = mod.second.base;
            Address_type curEnd = mod.second.base + mod.second.size;
            if (mod.second.elfFile->GetImpl())
            {
                curAddress = mod.second.elfFile->GetImageBase();
                curEnd = mod.second.elfFile->GetImageEnd();
            }
            else if (!mod.second.size)
            {
                continue;
            }
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
            return InsertUnresolved(libName);
        }
    }
    CElfImportsLoader::ModuleIterator CElfImportsLoader::InsertUnresolved(const std::string& libName)
    {
        oui::String name;
        try { name = NormalizeName(libName); }
        catch (...) { name = orthia::Utf8ToPlatformString(libName); }

        auto it = m_mappedModules.find(name.native);
        if (it != m_mappedModules.end())
            return it;
        ModuleInfo info;
        info.fullName = name;
        info.elfFile = std::make_shared<CSimpleElfFile>();
        info.unresolved = true;
        try
        {
            // one page of its own: at 0 all the unresolved dependencies would collide in tbl_modules
            info.size = 0x1000;
            info.base = ReserveSpace(info.size);
        }
        catch (std::exception& reserveError)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "Can't reserve space for ", libName, ": ", reserveError.what());
            info.size = 0;
        }
        return m_mappedModules.insert({ name.native, info }).first;
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
        if (fullName.native.empty())
        {
            ORTHIA_LOG(orthia::LogSeverity::Warning, "ELF dependency not found: ", libName);
            return InsertUnresolved(libName);
        }

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
        if (mappedElf->GetDianaArch() != m_dianaArch)
        {
            throw std::runtime_error("Architecture mismatch: " + orthia::PlatformStringToUtf8(fullName.native));
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
        info.base = mappedElf->GetImageBase();
        info.size = mappedElf->GetMappedFile().size();
        QueryImageIdentity(*mappedElf, info.identity);
        {
            auto sha1 = CalcSha1(rawFile);
            info.identity.sha1Hex = BytesToHex(sha1.data(), sha1.size());
        }
        return m_mappedModules.insert({ normalName.native, info }).first;
    }

    void CElfImportsLoader::LoadExports(ModuleInfo& mod)
    {
        if (!mod.elfFile->GetImpl())
            return;

        std::vector<orthia::NameInfo> found;
        if (!QueryMappedFileNames(*FindFormatById("elf"), *mod.elfFile, found))
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "Can't parse the module headers: ", mod.fullName.native);
            return;
        }
        // one name per address: the first, so $ifunc_memcpy wins over memcpy at the same address
        for (auto& info : found)
        {
            mod.names.insert({ info.address, std::move(info) });
        }
    }
    // Links the module's PLT slots to the exports of the loaded chain and records each import
    // at its slot (the absolute address), with the address the slot now holds; a reopen writes
    // the recorded values back (ImportsKeyedBySlot)
    void CElfImportsLoader::LinkImports(ModuleInfo& mod)
    {
        if (!mod.elfFile->GetImpl())
            return;

        struct Resolver : public diana::CBasePeLinkImportsObserver
        {
            CElfImportsLoader& loader;
            ModuleInfo& m_mod;
            Resolver(CElfImportsLoader& l, ModuleInfo& m) : loader(l), m_mod(m) {}
            void QueryFunctionByOrdinal(const char*, DI_UINT32, OPERAND_SIZE*) override {}
            void QueryFunctionByName(const char* pDllName, const char* pFunctionName,
                DI_UINT32, OPERAND_SIZE* pAddress) override
            {
                if (!pFunctionName || !*pFunctionName || !pAddress)
                    return;
                OPERAND_SIZE target = 0;
                for (auto& m : loader.m_mappedModules)
                {
                    if (!m.second.elfFile->GetImpl())
                        continue;
                    target = m.second.elfFile->DiGetProcAddress(pFunctionName);
                    if (target)
                        break;
                }
                // the slot: a module offset reported just before this call
                const Address_type slot = m_mod.base + GetLastAddress();
                if (target)
                {
                    *pAddress = target;
                }
                else
                {
                    // unresolved: the slot keeps what the file has (the PLT stub, relocated)
                    target = ReadSlot(slot);
                }

                orthia::NameInfo info;
                info.flags   = orthia::NameInfo::flags_Import;
                info.address = target;
                info.name    = orthia::Utf8ToPlatformString(pDllName && *pDllName ?
                    std::string(pDllName) + "!" + pFunctionName : std::string(pFunctionName));
                m_mod.names.insert({ slot, info });
            }
            OPERAND_SIZE ReadSlot(Address_type slot) const
            {
                const auto& image = m_mod.elfFile->GetMappedFile();
                const int size = m_mod.elfFile->GetDianaMode();
                DI_UINT64 value = 0;
                if (slot >= m_mod.base && slot - m_mod.base + size <= image.size())
                {
                    memcpy(&value, image.data() + (size_t)(slot - m_mod.base), (size_t)size);
                }
                return value;
            }
        } resolver(*this, mod);

        int err = mod.elfFile->QueryImports(&resolver);
        if (err && err != DI_END)
        {
            ORTHIA_LOG(orthia::LogSeverity::Error, "QueryImports failed: ", err);
        }
    }

    void CElfImportsLoader::LoadModules(
        const oui::String& fileName,
        std::shared_ptr<ISimpleFile> file,
        std::shared_ptr<oui::IFileSystem> pFs)
    {
        auto elfFile = std::dynamic_pointer_cast<orthia::CSimpleElfFile>(file);
        if (!elfFile)
        {
            throw std::runtime_error("Not an ELF image");
        }
        CheckCancel();
        m_pFs = pFs;
        m_dianaMode = elfFile->GetDianaMode();
        m_dianaArch = elfFile->GetDianaArch();
        m_freeSpaceStart = elfFile->GetImageEnd();
        m_exeDirectory = DirectoryOfFile(fileName);

        oui::String shortFileName;
        orthia::UnparseFileNameFromFullFileName(fileName.native, &shortFileName.native);

        ModuleInfo rootInfo;
        rootInfo.elfFile      = elfFile;
        rootInfo.originalFile = true;
        rootInfo.fullName     = fileName;
        rootInfo.base         = elfFile->GetImageBase();
        rootInfo.size         = elfFile->GetMappedFile().size();
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

        // Link root's imports against the fully loaded chain (patches GOT slots, records the names)
        auto rootFinal = m_mappedModules.find(rootKey);
        if (rootFinal != m_mappedModules.end())
        {
            try { LinkImports(rootFinal->second); }
            catch (std::exception& e)
            {
                ORTHIA_LOG(orthia::LogSeverity::Error, "Can't link ELF imports: ", e.what());
            }
        }
    }

    void CElfImportsLoader::InsertNames(std::shared_ptr<CModuleManager> moduleManager,
        const ModuleInfo& mod)
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        for (auto& name : mod.names)
        {
            InsertName(classicDatabase, mod.base, name.second, name.first);
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
                // one transaction: in autocommit every name is a separate disk sync
                auto batch = classicDatabase->BeginBatch();
                InsertNames(moduleManager, mod.second);
                batch->Commit();
                continue;
            }
            if (!mod.second.size)
            {
                // no range could be reserved: a module at 0 would collide with the others
                continue;
            }
            oui::String shortName;
            orthia::UnparseFileNameFromFullFileName(mod.second.fullName.native, &shortName.native);

            CAutoRollbackClassicDatabase rollback;
            classicDatabase->StartSaveModule(
                mod.second.base,
                mod.second.size,
                shortName.native,
                &rollback,
                true);

            int builtInFlags = orthia::ModuleInfo::builtInFlags_moduleTypeElf;
            ModuleSourceMeta source;
            if (mod.second.unresolved)
            {
                builtInFlags |= orthia::ModuleInfo::builtInFlags_unresolved;
                source.srcKind = g_meta_src_kind_none;
            }
            else
            {
                source.srcKind = g_meta_src_kind_linked;
                source.identity = &mod.second.identity;
            }
            InsertModuleMetaInfo(classicDatabase,
                mod.second.base,
                mod.second.fullName.native,
                0,
                builtInFlags,
                &source);

            InsertNames(moduleManager, mod.second);
            classicDatabase->DoneSave();
        }
    }
}
