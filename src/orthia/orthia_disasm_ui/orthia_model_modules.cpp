#include "orthia_model_modules.h"
#include "orthia_pe.h"
#include "diana_pe_cpp.h"
#include "orthia_streams.h"
#include "orthia_memory_cache.h"
#include "orthia_database_saver.h"
#include "orthia_item_file.h"

namespace orthia
{
    CImportsLoader::CImportsLoader(std::shared_ptr<oui::BaseOperation> operation)
        :
        m_operation(operation)
    {
    }
    void CImportsLoader::CheckCancel()
    {
        if (m_operation && m_operation->IsCancelled())
        {
            throw std::runtime_error("Cancelled");
        }
    }

    oui::String CImportsLoader::NormalizeName(const std::string& dllName)
    {
        oui::String str = orthia::Utf8ToPlatformString(dllName);
        return NormalizeName(str);
    }
    oui::String CImportsLoader::NormalizeName(const oui::String& str)
    {
        int platformError = 0;
        oui::String normalName;
        std::tie(platformError, normalName) = m_pFs->SyncNormalizeName(str, true);
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't normalize name: " + orthia::PlatformStringToUtf8(str.native), platformError);
        }
        return normalName;
    }
    oui::String CImportsLoader::LocateFile(const oui::String& dllName)
    {
        int platformError = 0;
        oui::String normalName;
        std::tie(platformError, normalName) = m_pFs->SyncLocateFile(dllName, m_dianaMode);
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't locate name: " + orthia::PlatformStringToUtf8(dllName.native), platformError);
        }
        return normalName;
    }
    bool CImportsLoader::CheckConflicts(std::shared_ptr<orthia::CSimplePeFile> peFile)
    {
        auto modAddress = peFile->GetImageBase();
        auto modEnd = peFile->GetImageEnd();

        if (!modAddress)
        {
            return true;
        }
        for (auto& mod : m_mappedModules)
        {
            auto curAddress = mod.second.peFile->GetImageBase();
            auto curEnd = mod.second.peFile->GetImageEnd();

            if (curAddress > modAddress &&
                curAddress < modEnd)
            {
                return true;
            }
            if (curEnd > modAddress && 
                curEnd < modEnd)
            {
                return true;
            }
            if (curAddress <= modAddress &&
                curEnd >= modEnd)
            {
                return true;
            }
            if (modAddress <= curAddress &&
                modEnd >= curEnd)
            {
                return true;
            }
        }
        return false;
    }
    static OPERAND_SIZE RoundUp(OPERAND_SIZE lastPossibleAddress, OPERAND_SIZE alignment)
    {
        if ((lastPossibleAddress % alignment) == 0)
        {
            return lastPossibleAddress;
        }

        auto newAddress = lastPossibleAddress - (lastPossibleAddress % alignment);
        return newAddress + alignment;
    }

    void CImportsLoader::RelocateModule(std::shared_ptr<orthia::CSimplePeFile> peFile)
    {
        // check 
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

        if (m_freeSpaceStart > lastPossibleAddress)
        {
            throw std::runtime_error("Can't load module");
        }

        auto freeSpaceSize = lastPossibleAddress - m_freeSpaceStart;
        if (freeSpaceSize < peFile->GetImageEnd() || peFile->GetImageEnd() > lastPossibleAddress)
        {
            throw std::runtime_error("Can't load module");
        }
        auto possibleAddress = RoundUp(m_freeSpaceStart, 0x10000);
        peFile->Relocate(possibleAddress);
    }
    CImportsLoader::ModuleIterator CImportsLoader::LoadModule(const std::string& dllName)
    {
        auto name = NormalizeName(dllName);
        {
            auto it = m_mappedModules.find(name.native);
            if (it != m_mappedModules.end())
            {
                return it;
            }
        }
        auto fullName = LocateFile(name);

        int platformError = 0;
        std::shared_ptr<oui::IFile2> file;
        std::tie(platformError, file) = m_pFs->SyncOpenFile(oui::FileUnifiedId(fullName));
        if (platformError)
        {
            throw orthia::CWin32Exception("Can't open file: " + orthia::PlatformStringToUtf8(fullName.native), platformError);
        }

        std::vector<char> binPeFile;
        oui::String error = ReadFileToVector(file, binPeFile);
        if (!error.native.empty())
        {
            throw orthia::CWin32Exception("Can't read file: " + orthia::PlatformStringToUtf8(fullName.native) + "\n" + orthia::PlatformStringToUtf8(error.native), platformError);
        }

        auto mappedPE = std::make_shared<orthia::CSimplePeFile>();
        orthia::MapFileParameters params;
        params.mapFlags = DIANA_PE_MAP_DO_NOT_RELOCATE;
        mappedPE->MapFile(binPeFile, params);

        if (CheckConflicts(mappedPE))
        {
            RelocateModule(mappedPE);
        }
        else
        {
            mappedPE->Relocate(mappedPE->GetImageBase());
        }
        ModuleInfo info;
        info.peFile = mappedPE;
        info.fullName = fullName;
        if (m_freeSpaceStart < mappedPE->GetImageEnd())
        {
            m_freeSpaceStart = mappedPE->GetImageEnd();
        }
        return m_mappedModules.insert({ name.native, info }).first;
    }

    void CImportsLoader::QueryFunctionByOrdinal(const char* pDllName,
        DI_UINT32 ordinal,
        OPERAND_SIZE* pAddress)
    {
        QueryFunctionImpl(pDllName, "", ordinal, pAddress);
    }
    void CImportsLoader::QueryFunctionByName(const char* pDllName,
        const char* pFunctionName,
        DI_UINT32 hint,
        OPERAND_SIZE* pAddress)
    {
        QueryFunctionImpl(pDllName, pFunctionName, DI_MAX_OPERAND_SIZE, pAddress);
    }
    void CImportsLoader::QueryFunctionImpl(const char* pDllName,
        const char* pFunctionName,
        OPERAND_SIZE ordinalIn,
        OPERAND_SIZE* pAddress)
    {
        try
        {
            if (!pDllName || !pFunctionName || !pAddress)
            {
                throw std::runtime_error("Invalid argument");
            }

            std::string dllName(pDllName);
            std::string functionName(pFunctionName);
            OPERAND_SIZE ordinal = ordinalIn;

            const int maxTryCount = 3;
            for (int i = 0; i < maxTryCount; ++i)
            {
                ModuleIterator it = LoadModule(dllName);

                auto ordinalToPass = ordinal;
                if (ordinalToPass == DI_MAX_OPERAND_SIZE)
                {
                    ordinalToPass = DIANA_PE_INVALID_ORDINAL_VALUE;
                }

                OPERAND_SIZE forwardOffset = 0;
                *pAddress = it->second.peFile->DiGetProcAddress(functionName.c_str(), &forwardOffset, (DI_UINT16)ordinalToPass);

                if (!forwardOffset)
                {
                    orthia::NameInfo nameInfo;
                    nameInfo.flags = NameInfo::flags_Import;
                    nameInfo.name.native = orthia::Utf8ToPlatformString(dllName + "!" + functionName);
                    nameInfo.address = *pAddress;
                    m_currentModule->second.names.insert({ GetLastAddress() , nameInfo });
                    return;
                }

                auto fwString = it->second.peFile->DiReadForwardingString(forwardOffset);
                DI_CHECK_CPP(diana::ParseForwarderString(fwString, dllName, functionName, ordinal));
            }
            throw std::runtime_error("Can't process forwarding");
        }
        catch (const std::exception& e)
        {
            oui::LogOutput(oui::LogFlags::Error, e.what());
        }
        CheckCancel();
    }

    void CImportsLoader::LoadImports(ModuleInfo& mod)
    {
        struct ImportsCollector :public diana::CBasePeLinkImportsObserver
        {
            ModuleInfo& m_mod;

            ImportsCollector(ModuleInfo& mod)
                : m_mod(mod)
            {
            }
            void QueryFunctionByOrdinal(const char* pDllName,
                DI_UINT32 ordinal,
                OPERAND_SIZE* pAddress)
            {
            }
            void QueryFunctionByName(const char* pDllName,
                const char* pFunctionName,
                DI_UINT32 hint,
                OPERAND_SIZE* pAddress)
            {
                if (!*pAddress)
                {
                    return;
                }
                orthia::Address_type fncAddress = m_mod.peFile->GetImageBase() + *pAddress;
                orthia::NameInfo nameInfo;
                nameInfo.flags = orthia::NameInfo::flags_Import;
                nameInfo.address = fncAddress;
                nameInfo.name = orthia::Utf8ToPlatformString(pFunctionName);
                m_mod.names.insert({ GetLastAddress() , nameInfo });
            }
        } collector(mod);
        DI_CHECK_CPP(mod.peFile->QueryImports(&collector));
    }
    void CImportsLoader::LoadExports(ModuleInfo& mod)
    {
        struct ExportsCollector :public diana::CBasePeLinkImportsObserver
        {
            ModuleInfo& m_mod;

            ExportsCollector(ModuleInfo& mod)
                : m_mod(mod)
            {
            }
            void QueryFunctionByOrdinal(const char* pDllName,
                DI_UINT32 ordinal,
                OPERAND_SIZE* pAddress)
            {
            }
            void QueryFunctionByName(const char* pDllName,
                const char* pFunctionName,
                DI_UINT32 hint,
                OPERAND_SIZE* pAddress)
            {
                if (!*pAddress)
                {
                    return;
                }
                orthia::Address_type fncAddress = m_mod.peFile->GetImageBase() + *pAddress;
                orthia::NameInfo nameInfo;
                nameInfo.flags = orthia::NameInfo::flags_Export;
                nameInfo.address = fncAddress;
                nameInfo.name = orthia::Utf8ToPlatformString(pFunctionName);
                m_mod.names.insert({ GetLastAddress() , nameInfo });
            }
        } collector(mod);
        DI_CHECK_CPP(mod.peFile->QueryExports(&collector));

        OPERAND_SIZE entryPoint = mod.peFile->GetImpl()->mappedPE.pImpl->addressOfEntryPoint;
        if (entryPoint)
        {
            entryPoint += mod.peFile->GetImageBase();
            collector.QueryFunctionByName("$", "$entrypoint", 0, &entryPoint);
        }
        // report tls callbacks
        std::vector<OPERAND_SIZE> tlsCallbacks;
        mod.peFile->QueryTLSCallbacks(tlsCallbacks);
        for (int i = 0, size = (int)tlsCallbacks.size(); i < size; ++i)
        {
            auto name = "$tls_" + orthia::ToAnsiStringAsHex((unsigned short)i);
            auto callback = tlsCallbacks[i];
            collector.QueryFunctionByName("$", name.c_str(), 0, &callback);
        }

    }
    // CImportsLoader
    void CImportsLoader::LoadModules(const oui::String & fileName, 
        std::shared_ptr<orthia::CSimplePeFile> peFile,
        std::shared_ptr<oui::IFileSystem> pFs)
    {
        CheckCancel();
        m_pFs = pFs;
        if (!m_pFs)
        {
            throw std::runtime_error("Unknown filesystem");
        }
        m_dianaMode = peFile->GetImpl()->mappedPE.pImpl->dianaMode;

        oui::String shortFileName;
        orthia::UnparseFileNameFromFullFileName(fileName.native, &shortFileName.native);
        ModuleInfo info;
        info.peFile = peFile;
        info.originalFile = true;
        info.fullName = fileName;
        auto res = m_mappedModules.insert(std::make_pair(NormalizeName(shortFileName).native, info));
        m_currentModule = res.first;
        if (m_freeSpaceStart < peFile->GetImageEnd())
        {
            m_freeSpaceStart = peFile->GetImageEnd();
        }

        auto imageBase = peFile->GetImageBase();

        orthia::CReaderOverVector reader(imageBase, peFile->GetMappedPeFile());
        orthia::CMemoryStorageOfModifiedData mappedFile(&reader);
        orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);

        std::vector<char> page(4096);
        DI_CHECK_CPP(DianaPeFile_LinkImports(&peFile->GetImpl()->mappedPE,
            imageBase,
            &writeStream,
            &page.front(),
            (ULONG)page.size(),
            GetParent()));

        for (auto& pair: m_mappedModules)
        {
            LoadExports(pair.second);
            if (pair.second.originalFile)
            {
                continue;
            }
            LoadImports(pair.second);
        }
    }
    void CImportsLoader::InsertNames(std::shared_ptr<CModuleManager> moduleManager, const ModuleInfo& mod)
    {
        auto classicDatabase = moduleManager->QueryDatabaseManager()->GetClassicDatabase();
        for (auto& name : mod.names)
        {
            InsertName(classicDatabase, mod.peFile->GetImageBase(), name.second, name.first);
        }
    }

    void CImportsLoader::ReportModules(std::shared_ptr<CModuleManager> moduleManager)
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
            classicDatabase->StartSaveModule(mod.second.peFile->GetImageBase(),
                mod.second.peFile->GetMappedPeFile().size(), 
                shortName.native,
                &rollback);

            InsertModuleMetaInfo(classicDatabase,
                mod.second.peFile->GetImageBase(),
                mod.second.fullName.native);

            InsertNames(moduleManager, mod.second);

            // add metainfo
            // 1. flags
            // 2. fullname
            // 3. import functions address
            classicDatabase->DoneSave();
        }
    }
}
