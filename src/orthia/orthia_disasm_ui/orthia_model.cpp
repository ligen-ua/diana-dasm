#include "orthia_model.h"
#include "orthia_files.h"
#include "orthia_helpers.h"
#include "orthia_database_module.h"
#include "orthia_item_process.h"
#include "orthia_item_file.h"
#include "orthia_model_modules.h"
#include "orthia_log.h"
#include "orthia_module_symbols.h"
#include "orthia_shellcode.h"
#include "orthia_diana_arch.h"
#include "orthia_common_format.h"
#include "orthia_databases.h"
#include "formats/orthia_image_format.h"

namespace orthia
{
    const unsigned long long g_maxSizeBytes = 512 * 1024 * 1024;
    const unsigned long long g_minSizeBytes = 1;

    oui::String ReadFileToVector(std::shared_ptr<oui::IFile> file, std::vector<char>& binPeFile, std::shared_ptr<oui::BaseOperation> operation, intrusive_ptr<CTextNode> errorNode)
    {
        if (!errorNode)
        {
            errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        }

        int error = 0;
        unsigned long long fileSize = 0;
        std::tie(error, fileSize) = file->GetSizeInBytes();
        if (fileSize < g_minSizeBytes)
        {
            return errorNode->QueryValue(ORTHIA_TCSTR("empty"));
        }
        if (fileSize > g_maxSizeBytes)
        {
            return errorNode->QueryValue(ORTHIA_TCSTR("too-big"));
        }

        error = file->ReadExact(operation, 0, (size_t)fileSize, binPeFile);
        if (error)
        {
            return oui::GetErrorText(error);
        }
        return oui::String();
    }

    // CProgramModel
    CProgramModel::CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config)
        :
            m_config(config)
    {
        m_fileSystem = std::make_shared<oui::CFileSystem>();
        m_processSystem = std::make_shared<oui::CProcessSystem>();
        m_commandProcessor = std::make_shared<orthia::CCommandProcessor>();
    }
    
    std::shared_ptr<IWorkPlaceItem> CProgramModel::GetActiveItem()
    {
        WorkplaceItem item;
        if (!QueryActiveWorkspaceItem(item)) {
            return nullptr;
        }
        return GetItem(item.uid);
    }
    std::shared_ptr<IWorkPlaceItem> CProgramModel::GetItem(int uid)
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);

        auto it = m_items.find(uid);
        if (it == m_items.end())
        {
            return nullptr;
        }
        return it->second;
    }
    void CProgramModel::SetUILog(std::shared_ptr<IUILogInterface> uiLog)
    {
        m_uiLog = uiLog;
        m_analyzer.Init(uiLog, m_config, m_fileSystem);
    }
    std::shared_ptr<oui::CFileSystem> CProgramModel::GetFileSystem()
    {
        return m_fileSystem;
    }
    std::shared_ptr<oui::CProcessSystem> CProgramModel::GetProcessSystem()
    {
        return m_processSystem;
    }
    std::shared_ptr<orthia::CCommandProcessor> CProgramModel::GetCommandProcessor()
    {
        return m_commandProcessor;
    }
    bool CProgramModel::QueryWorkspaceItem(int id, WorkplaceItem& item) const
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);

        auto it = m_items.find(id);
        if (it == m_items.end())
        {
            return false;
        }
        item.uid = id;
        item.name = it->second->GetShortName();
        return true;
    }
    bool CProgramModel::QueryActiveWorkspaceItem(WorkplaceItem& item) const
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);

        auto it = m_items.find(m_activeId);
        if (it == m_items.end())
        {
            return false;
        }
        item.uid = m_activeId;
        item.name = it->second->GetShortName();
        return true;
    }
    bool CProgramModel::SetActiveItem(int uid)
    {
        // fire UI subscribers here
        std::vector<std::shared_ptr<IUIEventHandler>> handlers;
        int oldUid = 0;
        {
            std::unique_lock<std::mutex> lockGuard(m_lock);
            if (m_activeId == uid)
            {
                return false;
            }
            oldUid = m_activeId;
            handlers.assign(m_handlers.begin(), m_handlers.end());
        }
        if (oldUid)
        {
            for (auto& handler : handlers)
            {
                handler->OnPreWorkspaceItemChange(oldUid);
            }
        }
        // switch state
        {
            std::unique_lock<std::mutex> lockGuard(m_lock);

            auto it = m_items.find(uid);
            if (it == m_items.end())
            {
                return false;
            }
            m_activeId = uid;
        }
        // fire UI subscribers here
        for (auto& handler : handlers)
        {
            handler->OnWorkspaceItemChanged(uid);
        }
        return true;
    }
    void CProgramModel::Stop()
    {
        m_fileSystem->Stop();
        m_analyzer.Stop();
    }

    void CProgramModel::SubscribeUI(std::shared_ptr<IUIEventHandler> handler)
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);
        m_handlers.insert(handler);
    }

    void CProgramModel::UnsubscribeUI(std::shared_ptr<IUIEventHandler> handler)
    {
        std::unique_lock<std::mutex> lockGuard(m_lock);
        m_handlers.erase(handler);
    }

    int CProgramModel::QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const
    {
        items.clear();
        std::unique_lock<std::mutex> lockGuard(m_lock);

        int activePos = -1;
        for (const auto& item : m_items)
        {
            WorkplaceItem res;
            res.uid = item.first;
            res.name = item.second->GetShortName();
            if (res.uid == m_activeId)
            {
                activePos = (int)items.size();
            }
            items.push_back(res);
        }
        return activePos;
    }

    std::map<PlatformString_type, PlatformString_type> CProgramModel::QueryOpenDatabaseFolders() const
    {
        std::map<PlatformString_type, PlatformString_type> result;
        std::unique_lock<std::mutex> lockGuard(m_lock);
        for (const auto& item : m_items)
        {
            auto folder = item.second->GetDatabaseFolder();
            if (!folder.empty())
            {
                result[folder] = item.second->GetShortName().native;
            }
        }
        return result;
    }

    int CProgramModel::RegisterItem(std::shared_ptr<IWorkPlaceItem> item, bool makeActive)
    {
        int newItemId = 0;
        {
            std::unique_lock<std::mutex> lockGuard(m_lock);
            m_items[++m_lastUid] = item;
            newItemId = m_lastUid;
        }
        if (makeActive)
        {
            this->SetActiveItem(newItemId);
        }
        return newItemId;
    }
    void CProgramModel::WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line)
    {
        thread->AddTask([=]() {
            if (auto log = m_uiLog.lock())
            {
                log->WriteLog(line);
            }
        });
    }


    void CProgramModel::CreateProcItemFS(std::shared_ptr<oui::IProcess> proc,
        oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
        intrusive_ptr<CTextNode> mainNode,
        intrusive_ptr<CTextNode> errorNode,
        std::shared_ptr<CProcessWorkplaceItem> info)
    {
        try
        {
            // the start in the name: a later process with the same pid gets its own folder
            const auto startInfo = proc->GetStartInfo();
            const auto processName = proc->GetFullFileNameForUI();
            auto folderName = GetFileSystem()->SyncSanitizeName(oui::String(
                MakeProcFolderName(processName.native, startInfo.IsKnown() ? startInfo.unixSeconds : 0)));
            auto dbFolder = m_config->GetProcDBFolder() + AddSlash2(folderName.native);
            auto dbFileName = dbFolder + m_config->GetDBFileName();
            auto readmeFileName = dbFolder + m_config->GetReadmeFileName();
            auto metaFileName = dbFolder + m_config->GetMetaFileName();
            CreateAllDirectoriesForFile(dbFileName);
            info->SetProcFolder(dbFolder);

            // the database of an earlier open is reused only for the same process: the folder
            // name has the start in seconds, meta.xml has it exactly
            bool reuse = false;
            if (!m_config->GetOverwriteDatabases() && startInfo.IsKnown() && IsFileExist(dbFileName))
            {
                FolderMeta saved;
                reuse = ReadFolderMeta(metaFileName, saved) &&
                    IsFolderReadable(saved) &&
                    saved.kind == g_folderKindProcess &&
                    saved.startKey == startInfo.key;
            }

            // save readme file 
            orthia::CFile readmeFile;
            readmeFile.Open_Silent(readmeFileName, g_desired_write, g_share_read, g_create_always);

            auto readmeHeader = mainNode->QueryValue(ORTHIA_TCSTR("readme-header"));
            auto originalName = oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("original-name")),
                proc->GetFullFileNameForUI());

            std::stringstream textInfo;
            textInfo << PlatformStringToUtf8(readmeHeader) << "\n";
            textInfo << PlatformStringToUtf8(originalName.native) << "\n";
            auto str = textInfo.str();
            readmeFile.WriteToFile_Silent(str.c_str(), str.size());

            WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("database-file")),
                dbFileName));
            if (reuse)
            {
                WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("database-reused")));
            }

            // done readme, create db
            auto persistentItemStorage = std::make_shared<CFilePersistentItemStorage>();
            auto moduleManager = std::make_shared<CModuleManager>();
            moduleManager->Reinit(dbFileName, !reuse);

            if (!reuse)
            {
                FolderMeta meta;
                meta.kind = g_folderKindProcess;
                meta.dianaMode = proc->GetDianaMode();
                meta.dianaArch = proc->GetDianaArch();
                meta.pid = ParseProcFolderPid(processName.native);
                meta.name = processName.native;
                meta.startUnix = startInfo.unixSeconds;
                meta.startKey = startInfo.key;
                WriteFolderMeta(metaFileName, meta);
            }

            persistentItemStorage->Init(moduleManager->QueryDatabaseManager());

            auto moduleStorage = std::make_shared<ModuleStorage>(
                moduleManager->QueryDatabaseManager()->GetClassicDatabase());
            info->Init(moduleManager, persistentItemStorage, moduleStorage);
            persistentItemStorage->CPersistentItemStorage::Init(info);
            if (reuse)
            {
                // the analysis saved by the earlier open, for the modules that are still the same
                info->LoadSavedModuleFlags();
            }
        }
        catch (std::exception& e)
        {
            WriteLog(completeHandler->GetThread(), oui::PassParameter1(errorNode->QueryValue(ORTHIA_TCSTR("cant-create-fs")),
                proc->GetFullFileNameForUI()));

            WriteLog(completeHandler->GetThread(), oui::String(orthia::Utf8ToPlatformString(e.what())));
        }
    }
    // "x64", "arm64", "thumb", ... for the line logged when a target is opened
    static oui::String GetArchNameForUI(int dianaMode, int dianaArch)
    {
        if (const DianaArchChoice* choice = FindDianaArchChoice(dianaMode, dianaArch))
        {
            return Utf8ToPlatformString(choice->name);
        }
        return dianaMode == DIANA_MODE16 ? ORTHIA_TCSTR("x86-16") : ORTHIA_TCSTR("?");
    }

    // "base X, entry Y, size Z"; an entry point at the base is none (a PE without one, raw code).
    // A .NET image says so first: that is why an IL-only one has no entry
    static oui::String FormatImageLayoutForUI(const intrusive_ptr<CTextNode>& mainNode,
        Address_type base, Address_type entryPoint, Address_type size, int dianaMode, int builtInFlags)
    {
        const oui::String noValue(ORTHIA_TCSTR("-"));
        oui::String layout = oui::PassParameter3(mainNode->QueryValue(ORTHIA_TCSTR("opened-layout")),
            base ? oui::String(AddressToString(base, dianaMode)) : noValue,
            entryPoint && entryPoint != base ? oui::String(AddressToString(entryPoint, dianaMode)) : noValue,
            size ? oui::String(ToWideStringAsHex_Short((unsigned long long)size)) : noValue);
        if (builtInFlags & ModuleInfo::builtInFlags_managed)
        {
            layout = oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("opened-managed")), layout);
        }
        return layout;
    }

    void CProgramModel::AddProcess(std::shared_ptr<oui::IProcess> proc,
        oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler)
    {
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
        auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        oui::fsui::OpenResult result;
        result.error = errorNode->QueryValue(ORTHIA_TCSTR("unknown"));
        oui::ScopedGuard handlerGuard([&]() {
            completeHandler->Reply(completeHandler, proc, result);
        });

        // opening
        WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("opening")),
            proc->GetFullFileNameForUI()));

        int platformError = 0;
        unsigned long long fileSize = 0;
        std::tie(platformError, fileSize) = proc->GetSizeInBytes();
        int dianaMode = proc->GetDianaMode();
        int dianaArch = proc->GetDianaArch();

        WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("analyzing-file")));

        auto persistentItemStorage = std::make_shared<CPersistentItemStorage>();
        auto info = std::make_shared<CProcessWorkplaceItem>(proc, proc->GetFullFileNameForUI(), dianaMode, dianaArch,
            persistentItemStorage);
        info->ReloadModules();
        persistentItemStorage->Init(info);

        CreateProcItemFS(proc, completeHandler, mainNode, errorNode, info);

        WorkAddressRangeInfo mainModule;
        int mainModuleFlags = 0;
        if (auto address = info->GetMainModuleAddress())
        {
            mainModule = info->GetRangeInfo(address);
            auto addressToStart = std::max(mainModule.entryPoint, mainModule.address);
            result.extraInfo[model_OpenResult_extraInfo_InitalAddress] = std::any(addressToStart);

            std::vector<ModuleInfo> modules;
            info->GetModules(modules);
            // the main module address of a process is its entry point
            for (const auto& mod : modules)
            {
                if (mod.IsInRange(address))
                {
                    mainModuleFlags = mod.builtInFlags;
                    break;
                }
            }
        }

        auto workspaceId = RegisterItem(info, false);
        result.extraInfo[model_OpenResult_extraInfo_WorkspaceId] = std::any(workspaceId);

        WriteLog(completeHandler->GetThread(), oui::PassParameter2(mainNode->QueryValue(ORTHIA_TCSTR("opened-process")),
            GetArchNameForUI(dianaMode, dianaArch),
            FormatImageLayoutForUI(mainNode, mainModule.address, mainModule.entryPoint, mainModule.size, dianaMode, mainModuleFlags)));

        // OK
        result.error.native.clear();

        EnqueueAnalysisOps(completeHandler->GetThread(), workspaceId, info, info->GetMainModuleAddress(), info);
    }
    void CProgramModel::NotifyWorkspaceDataRefreshed(std::shared_ptr<oui::CWindowThread> uiThread, int workspaceId)
    {
        std::vector<std::shared_ptr<IUIEventHandler>> handlers;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            handlers.assign(m_handlers.begin(), m_handlers.end());
        }
        uiThread->AddTask([handlers = std::move(handlers), workspaceId]() {
            for (auto& h : handlers)
                h->OnWorkspaceDataRefreshed(workspaceId);
        });
    }

    void CProgramModel::EnqueueAnalysisOps(
        std::shared_ptr<oui::CWindowThread> uiThread,
        int workspaceId,
        std::shared_ptr<IWorkPlaceItem> item,
        Address_type mainAddr,
        std::shared_ptr<CProcessWorkplaceItem> procItem)
    {
        if (!m_config->GetDeepAnalysis())
        {
            // quick open (--cmd without --analyze): .analyze and .reload still work on demand
            return;
        }
        if (procItem)
        {
            // Phase 1 (process only): disassemble the main module and log a completion message.
            auto bgOp = std::make_shared<oui::BaseOperation>(uiThread);
            m_analyzer.EnqueueAnalyze(procItem, bgOp, mainAddr, workspaceId,
                [uiThread, uiLog = m_uiLog]() {
                    uiThread->AddTask([uiLog]() {
                        if (auto log = uiLog.lock())
                        {
                            auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                            log->WriteLog(node->QueryValue(ORTHIA_TCSTR("analysis-complete")));
                        }
                    });
                });
        }

        // Phase 2: load debug symbols; on completion, re-analyze with private symbols,
        // then notify all UI subscribers that the workspace item changed.
        auto symOp = std::make_shared<oui::BaseOperation>(uiThread);
        m_analyzer.EnqueueLoadSymbols(item, symOp, workspaceId,
            [this, uiThread, workspaceId, mainAddr, item]() {

                NotifyWorkspaceDataRefreshed(uiThread, workspaceId);

                auto reanalyzeOp = std::make_shared<oui::BaseOperation>(uiThread);
                m_analyzer.EnqueueAnalyzePrivateSymbols(item, reanalyzeOp, mainAddr, workspaceId,
                    [this, uiThread, workspaceId]() {
                        NotifyWorkspaceDataRefreshed(uiThread, workspaceId);
                    });
            },
            [this, uiThread, workspaceId]() {
                NotifyWorkspaceDataRefreshed(uiThread, workspaceId);
            },
            procItem ? Address_type{0} : mainAddr);
    }

    static bool IsModuleAnalyzed(const std::shared_ptr<IWorkPlaceItem>& item, Address_type moduleAddress)
    {
        std::vector<ModuleInfo> modules;
        item->GetModules(modules);
        for (const auto& mod : modules)
        {
            if (mod.address == moduleAddress)
            {
                return (mod.flags & ModuleInfo::flags_analyzeDone) != 0;
            }
        }
        return false;
    }

    // The format and MapFileParameters of a file opened by hand (raw code), saved next to its
    // database so that a reopen doesn't ask again. A file without "format" is from before
    // formats were saved: raw code then
    static bool ReadOpenParameters(const PlatformString_type& fileName, const IImageFormat*& format, MapFileParameters& params)
    {
        orthia::CFile paramsFile;
        if (paramsFile.Open_Silent(fileName, g_desired_read, g_share_read, g_open_existing) != 0)
        {
            return false;
        }
        const auto size = paramsFile.GetSize();
        if (size <= 0 || size >= 4096)
        {
            return false;
        }
        std::vector<char> data((size_t)size);
        orthia::CCommonFormatParser parser;
        if (paramsFile.ExactRead_Silent(data.data(), (unsigned int)size) != 0 || !parser.Parse(data, false))
        {
            return false;
        }
        std::string formatId;
        unsigned long long base = 0;
        int mode = 0;
        // saved before ARM: x86
        int arch = DIANA_ARCH_X86;
        if (!parser.QueryMetadata(std::string("format"), &formatId) || formatId.empty())
        {
            formatId = "raw";
        }
        parser.QueryMetadata("base_address", &base);
        parser.QueryMetadata("diana_mode", &mode);
        parser.QueryMetadata("diana_arch", &arch);
        const IImageFormat* saved = FindFormatById(formatId);
        if (!saved || !mode || !FindDianaArchChoice(mode, arch))
        {
            return false;
        }
        format = saved;
        params.imageBase = base;
        params.dianaMode = mode;
        params.dianaArch = arch;
        return true;
    }

    static void WriteOpenParameters(const PlatformString_type& fileName, const IImageFormat& format, const MapFileParameters& params)
    {
        orthia::CFile paramsFile;
        if (paramsFile.Open_Silent(fileName, g_desired_write, g_share_read, g_create_always) == 0)
        {
            orthia::CCommonFormatBuilder builder;
            builder.AddMetadata(std::string("format"), std::string(format.GetId()));
            builder.AddMetadata("base_address", (unsigned long long)params.imageBase);
            builder.AddMetadata("diana_mode", params.dianaMode);
            builder.AddMetadata("diana_arch", params.dianaArch);
            std::vector<char> data;
            builder.Produce(&data);
            paramsFile.WriteToFile(data.data(), data.size());
        }
    }

    void CProgramModel::AddExecutable(std::shared_ptr<oui::IFile2> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
        const IImageFormat* forcedFormat,
        MapFileParameters params)
    {
        // non-ui thread
        oui::fsui::OpenResult result;
        oui::ScopedGuard handlerGuard([&]() {
            completeHandler->Reply(completeHandler, file, result);
        });
        try
        {
            // prepare the message on unknown error
            auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
            auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
            result.error = errorNode->QueryValue(ORTHIA_TCSTR("unknown"));

            // opening
            WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("opening")),
                file->GetFullFileNameForUI()));

            // read entire file in memory
            std::vector<char> binPeFile;
            result.error = ReadFileToVector(file, binPeFile, completeHandler, errorNode);
            if (!result.error.native.empty())
            {
                return;
            }

            if (completeHandler->IsCancelled())
            {
                handlerGuard.Release();
                return;
            }

            // the database folder
            auto fileHash = CalcSha1(binPeFile);
            auto fileHashStr = orthia::ToHexString(fileHash.data(), fileHash.size());
            auto dbFolder = m_config->GetDBFolder() + AddSlash2(fileHashStr);
            auto dbFileName = dbFolder + m_config->GetDBFileName();
            auto binFileName = dbFolder + m_config->GetBinFileName();
            auto readmeFileName = dbFolder + m_config->GetReadmeFileName();
            auto paramsFileName = dbFolder + m_config->GetParamsFileName();

            // the format asked for, the one the headers show, or the one saved when the
            // file was opened by hand before
            const IImageFormat* format = forcedFormat ? forcedFormat : FindFormatByData(binPeFile.data(), binPeFile.size());
            if (!format && !ReadOpenParameters(paramsFileName, format, params))
            {
                // the UI offers to open it as raw code
                result.error = errorNode->QueryValue(ORTHIA_TCSTR("unknown"));
                return;
            }
            auto mappedExe = format->MapFile(binPeFile, params);

            CreateAllDirectoriesForFile(dbFileName);
            WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("database-file")),
                dbFileName));
            if (forcedFormat && format->NeedsUserParameters())
            {
                WriteOpenParameters(paramsFileName, *format, params);
            }

            // check binary file
            bool hashIsValid = false;
            int error = 0;
            try
            {
                orthia::CFile existingFile;
                error = existingFile.Open_Silent(binFileName, g_desired_read, g_share_read, g_open_existing);
                if (!error)
                {
                    auto savedFileHash = CalcSha1(existingFile, completeHandler);
                    hashIsValid = savedFileHash == fileHash;
                }
            }
            catch (std::exception&)
            {
                hashIsValid = false;
            }

            WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("module-sha1")),
                fileHashStr));

            if (!hashIsValid)
            {
                // save file to local dir
                orthia::CFile localFile;
                error = localFile.Open_Silent(binFileName, g_desired_write, g_share_read, g_create_always);
                if (error)
                {
                    result.error = oui::PassParameter2(errorNode->QueryValue(ORTHIA_TCSTR("file-error-name-code")),
                        binFileName,
                        oui::GetErrorText(error));
                    return;
                }
                localFile.WriteToFile(binPeFile.data(), binPeFile.size());

                // save readme file 
                orthia::CFile readmeFile;
                error = readmeFile.Open_Silent(readmeFileName, g_desired_write, g_share_read, g_create_always);
                if (!error)
                {
                    auto readmeHeader = mainNode->QueryValue(ORTHIA_TCSTR("readme-header"));
                    auto originalName = oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("original-name")),
                        file->GetFullFileNameForUI());

                    std::stringstream textInfo;
                    textInfo << PlatformStringToUtf8(readmeHeader) << "\n";
                    textInfo << PlatformStringToUtf8(originalName.native) << "\n";
                    auto str = textInfo.str();
                    readmeFile.WriteToFile_Silent(str.c_str(), str.size());
                }
            }

            // fill the model data
            auto persistentItemStorage = std::make_shared<CFilePersistentItemStorage>();
            auto info = std::make_shared<FileWorkplaceItem>(persistentItemStorage);
            persistentItemStorage->CPersistentItemStorage::Init(info);

            info->fullName = file->GetFullFileName();
            info->databaseFolder = dbFolder;
            EraseLastSlash(info->databaseFolder);
            info->file = mappedExe;
            {
                oui::String shortName;
                orthia::UnparseFileNameFromFullFileName(info->fullName.native, &shortName.native);
                info->shortName = std::move(shortName);
            }
            const bool overwrite = m_config->GetOverwriteDatabases();
            const auto metaFileName = dbFolder + m_config->GetMetaFileName();
            FolderMeta savedMeta;
            const bool hasMeta = ReadFolderMeta(metaFileName, savedMeta);
            if (hasMeta && !overwrite && !IsFolderReadable(savedMeta))
            {
                throw std::runtime_error("The database of this file was made by a newer Orthia (" + savedMeta.orthiaVersion +
                    "), --overwrite-db replaces it with a new one: " + PlatformStringToUtf8(dbFolder));
            }
            info->moduleManager = std::make_shared<CModuleManager>();
            info->moduleManager->Reinit(dbFileName, overwrite);
            persistentItemStorage->Init(info->moduleManager->QueryDatabaseManager());

            if (info->shortName.native == m_config->GetBinFileName())
            {
                // opening own database, give more info
                std::vector<char> readmeBuffer;
                if (!orthia::LoadFileToVector_Silent(readmeFileName, readmeBuffer))
                {
                    readmeBuffer.push_back(0);
                    WriteLog(completeHandler->GetThread(), Utf8ToPlatformString(readmeBuffer.data()));
                }
            }

            const auto& mappedFile = info->file->GetMappedFile();
            if (mappedFile.empty())
            {
                result.error = errorNode->QueryValue(ORTHIA_TCSTR("empty"));
                return;
            }
            info->moduleLastValidAddress = mappedFile.size() - 1;
            if (Diana_SafeAdd(&info->moduleLastValidAddress, info->file->GetImageBase()))
            {
                result.error = errorNode->QueryValue(ORTHIA_TCSTR("invalid-image-base"));
                return;
            }

            CMemoryReaderOnLoadedData reader(info->file->GetImageBase(), mappedFile.data(), mappedFile.size());

            const Address_type imageBase = info->file->GetImageBase();
            const bool deepAnalysis = m_config->GetDeepAnalysis();
            const bool firstOpen = !info->moduleManager->QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(imageBase);
            const bool needsAnalysis = deepAnalysis && (firstOpen || !IsModuleAnalyzed(info, imageBase));
            if (firstOpen || !hasMeta)
            {
                FolderMeta meta;
                meta.kind = g_folderKindFile;
                meta.format = format->GetId();
                meta.dianaMode = mappedExe->GetDianaMode();
                meta.dianaArch = mappedExe->GetDianaArch();
                meta.imageBase = imageBase;
                meta.size = binPeFile.size();
                meta.sha1 = PlatformStringToUtf8(fileHashStr);
                WriteFolderMeta(metaFileName, meta);
            }
            // what the headers tell beyond the format (.NET)
            int extraBuiltInFlags = 0;
            if (auto mainImage = format->OpenModuleImage(reader, imageBase, mappedFile.size()))
            {
                extraBuiltInFlags = mainImage->GetExtraBuiltInFlags();
            }
            if (firstOpen || needsAnalysis)
            {
                // warn user it may take quite a time
                WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("analyzing-file")));
            }

            if (firstOpen)
            {
                bool importsKeyedBySlot = false;
                bool analyzed = false;
                {
                    auto dependencies = format->CreateDependencyLoader(completeHandler);
                    if (dependencies)
                    {
                        dependencies->LoadModules(file->GetFullFileName(), mappedExe, file->GetFileSystem());
                        importsKeyedBySlot = dependencies->ImportsKeyedBySlot();
                    }

                    analyzed = format->RegisterMainModule(*info->moduleManager, *mappedExe, reader, info->shortName.native, deepAnalysis);

                    if (dependencies)
                    {
                        dependencies->ReportModules(info->moduleManager);
                    }
                }
                ModuleSourceMeta source;
                source.iatSlots = importsKeyedBySlot;
                InsertModuleMetaInfo(info->moduleManager->QueryDatabaseManager()->GetClassicDatabase(),
                    imageBase,
                    info->fullName.native,
                    analyzed ? ModuleInfo::flags_analyzeDone : 0,
                    format->GetBuiltInFlag() | extraBuiltInFlags,
                    &source);
            }
            else
            {
                // a database made before these bits existed gets them now
                if (extraBuiltInFlags)
                {
                    UpdateModuleMetaInfo(info->moduleManager->QueryDatabaseManager()->GetClassicDatabase(), imageBase,
                        [&](CCommonFormatParser& parser, CCommonFormatBuilder& builder) {
                            int builtInFlags = 0;
                            parser.QueryMetadata("builtinflags", &builtInFlags);
                            if ((builtInFlags & extraBuiltInFlags) == extraBuiltInFlags)
                            {
                                return false;
                            }
                            builder.AddMetadata("builtinflags", builtInFlags | extraBuiltInFlags);
                            return true;
                        });
                }
                // the dependencies are not loaded again: the linked IAT comes from the database
                info->ApplyLinkedImports();
                if (needsAnalysis)
                {
                    // opened before without --analyze
                    info->moduleManager->AnalyzeRegisteredModule(imageBase, &reader, info->shortName.native, 0);
                    info->UpdateModuleFlags(imageBase, ModuleInfo::flags_analyzeDone, 0);
                }
            }
            info->InitImageSources(format);

            auto workspaceId = RegisterItem(info, false);
            result.extraInfo[model_OpenResult_extraInfo_WorkspaceId] = std::any(workspaceId);
            if (const Address_type initialAddress = format->GetInitialAddress(*mappedExe))
            {
                result.extraInfo[model_OpenResult_extraInfo_InitalAddress] = std::any(initialAddress);
            }

            const int dianaMode = mappedExe->GetDianaMode();
            WriteLog(completeHandler->GetThread(), oui::PassParameter3(mainNode->QueryValue(ORTHIA_TCSTR("opened-file")),
                Utf8ToPlatformString(format->GetDisplayName()),
                GetArchNameForUI(dianaMode, mappedExe->GetDianaArch()),
                FormatImageLayoutForUI(mainNode, imageBase, mappedExe->GetEntryPoint(), mappedFile.size(), dianaMode,
                    format->GetBuiltInFlag() | extraBuiltInFlags)));

            // OK
            result.error.native.clear();

            // kick off symbol loading and notify UI when complete
            EnqueueAnalysisOps(completeHandler->GetThread(), workspaceId, info, info->file->GetImageBase(), nullptr);
        }
        catch (std::exception& e)
        {
            result.error.native = orthia::Utf8ToPlatformString(e.what());
        }
    }

    void CProgramModel::AsyncOpenFileWithType(oui::ThreadPtr_type thread,
        const oui::FileUnifiedId& fileId,
        oui::FileWithTypeRecipientHandler_type resultCallback)
    {
        m_fileSystem->AsyncOpenFile(thread, fileId,
            [resultCallback, fileSystem = m_fileSystem, thread, config = m_config]
            (std::shared_ptr<oui::IFile2> file, int error, const oui::String& folderName)
            {
                if (error || !file || !folderName.native.empty())
                {
                    resultCallback(file, error, folderName, oui::OpenFileModelInfo());
                    return;
                }
                auto operation = std::make_shared<oui::Operation<oui::FileWithTypeRecipientHandler_type>>(
                    thread, resultCallback);
                fileSystem->AsyncExecute(thread,
                    [file, operation, config]()
                    {
                        const IImageFormat* format = nullptr;
                        oui::OpenFileModelInfo modelInfo;
                        auto [sizeErr, fileSize] = file->GetSizeInBytes();
                        if (!sizeErr && fileSize >= 2)
                        {
                            size_t bytesToRead = (size_t)std::min(fileSize, (unsigned long long)c_formatDetectSize);
                            std::vector<char> header;
                            if (file->ReadExact(nullptr, 0, bytesToRead, header) == 0)
                            {
                                format = FindFormatByData(header.data(), header.size());
                            }
                        }

                        if (format)
                        {
                            modelInfo.formatId = format->GetId();
                        }
                        else
                        {
                            std::vector<char> fullContent;
                            if (fileSize <= g_maxSizeBytes &&
                                file->ReadExact(nullptr, 0, (size_t)fileSize, fullContent) == 0)
                            {
                                auto fileHash = CalcSha1(fullContent);
                                auto fileHashStr = orthia::ToHexString(fileHash.data(), fileHash.size());
                                auto dbFolder = config->GetDBFolder() + AddSlash2(fileHashStr);
                                auto dbFileName = dbFolder + config->GetDBFileName();
                                // opened by hand before: AddExecutable reopens it the same way
                                const IImageFormat* saved = nullptr;
                                MapFileParameters savedParams;
                                if (orthia::IsFileExist(dbFileName) &&
                                    ReadOpenParameters(dbFolder + config->GetParamsFileName(), saved, savedParams))
                                {
                                    modelInfo.flags |= oui::model_flag_already_opened;
                                    modelInfo.dbFileName = dbFileName;
                                }
                            }
                        }
                        operation->ReplyWithRetain(operation, file, 0, oui::String(), std::move(modelInfo));
                    });
            });
    }

    bool CProgramModel::RemoveItem(int uid)
    {
        std::shared_ptr<IWorkPlaceItem> outOfLockLastItem;

        // Stage 1: mark item as delete-pending so no new ops get queued.
        oui::String itemName;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            auto it = m_items.find(uid);
            if (it != m_items.end())
            {
                itemName = it->second->GetShortName();
                it->second->SetDeletePending();
            }
        }
        if (!itemName.native.empty())
        {
            auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
            m_analyzer.WriteLog(oui::PassParameter1(node->QueryValue(ORTHIA_TCSTR("closing")), itemName));
        }
        // Stage 2: cancel all already-queued ops for this workspace item.
        m_analyzer.Cancel(uid);

        // Collect event handlers and determine the next active item.
        std::vector<std::shared_ptr<IUIEventHandler>> handlers;
        bool wasActive = false;
        int newActiveId = 0;
        {
            std::unique_lock<std::mutex> lock(m_lock);
            if (m_items.find(uid) == m_items.end())
            {
                return false;
            }
            wasActive = (m_activeId == uid);
            handlers.assign(m_handlers.begin(), m_handlers.end());

            if (wasActive)
            {
                auto it = m_items.upper_bound(uid);
                if (it != m_items.end())
                {
                    newActiveId = it->first;
                }
                else
                {
                    auto rit = m_items.lower_bound(uid);
                    if (rit != m_items.begin())
                    {
                        --rit;
                        newActiveId = rit->first;
                    }
                }
            }
        }

        // Let subscribers save UI state before the item disappears.
        if (wasActive)
        {
            for (auto& h : handlers)
            {
                h->OnPreWorkspaceItemChange(uid);
            }
        }

        // Remove the item and update the active ID atomically.
        {
            std::unique_lock<std::mutex> lock(m_lock);
            auto it = m_items.find(uid);
            if (it != m_items.end())
            {
                outOfLockLastItem = it->second;
                m_items.erase(it);
            }
            if (wasActive)
            {
                m_activeId = newActiveId;
            }
        }

        // Notify subscribers that this item was removed (state cleanup).
        for (auto& h : handlers)
        {
            h->OnWorkspaceItemRemoved(uid);
        }
        // Notify subscribers that the active item has changed (or 0 = none).
        if (wasActive)
        {
            for (auto& h : handlers)
            {
                h->OnWorkspaceItemChanged(newActiveId);
            }
        }
        return true;
    }
}