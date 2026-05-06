#include "orthia_model.h"
#include "orthia_files.h"
#include "orthia_helpers.h"
#include "orthia_database_module.h"
#include "orthia_item_process.h"
#include "orthia_item_file.h"
#include "orthia_model_modules.h"
#include "orthia_log.h"

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
        m_analyzer.Init(uiLog, m_config);
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
            auto folderName = GetFileSystem()->SyncSanitizeName(proc->GetFullFileNameForUI());
            auto dbFolder = m_config->GetProcDBFolder() + AddSlash2(folderName.native);
            auto dbFileName = dbFolder + m_config->GetDBFileName();
            auto binFileName = dbFolder + m_config->GetBinFileName();
            auto readmeFileName = dbFolder + m_config->GetReadmeFileName();
            CreateAllDirectoriesForFile(dbFileName);

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

            // done readme, create db
            auto persistentItemStorage = std::make_shared<CFilePersistentItemStorage>();
            auto moduleManager = std::make_shared<CModuleManager>();
            moduleManager->Reinit(dbFileName, false);

            persistentItemStorage->Init(moduleManager->QueryDatabaseManager());

            info->Init(moduleManager, persistentItemStorage);
        }
        catch (std::exception& e)
        {
            WriteLog(completeHandler->GetThread(), oui::PassParameter1(errorNode->QueryValue(ORTHIA_TCSTR("cant-create-fs")),
                proc->GetFullFileNameForUI()));

            WriteLog(completeHandler->GetThread(), oui::String(orthia::Utf8ToPlatformString(e.what())));
        }
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

        WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("analyzing-file")));

        auto persistentItemStorage = std::make_shared<CPersistentItemStorage>();
        auto info = std::make_shared<CProcessWorkplaceItem>(proc, proc->GetFullFileNameForUI(), dianaMode, persistentItemStorage);
        info->ReloadModules();

        CreateProcItemFS(proc, completeHandler, mainNode, errorNode, info);

        if (auto address = info->GerProcessModuleAddress())
        {
            auto rangeInfo = info->GetRangeInfo(address);
            auto addressToStart = std::max(rangeInfo.entryPoint, rangeInfo.address);
            result.extraInfo[model_OpenResult_extraInfo_InitalAddress] = std::any(addressToStart);
        }

        auto workspaceId = RegisterItem(info, false);
        result.extraInfo[model_OpenResult_extraInfo_WorkspaceId] = std::any(workspaceId);

        // OK
        result.error.native.clear();

        auto uiThread = completeHandler->GetThread();
        auto mainAddr = info->GerProcessModuleAddress();
        {
            auto bgOp = std::make_shared<oui::BaseOperation>(uiThread);

            m_analyzer.Enqueue(workspaceId, info, bgOp, mainAddr,
                [this, uiThread, workspaceId, info]() {
                    std::vector<std::shared_ptr<IUIEventHandler>> handlers;
                    {
                        std::unique_lock<std::mutex> lock(m_lock);
                        handlers.assign(m_handlers.begin(), m_handlers.end());
                    }
                    uiThread->AddTask(
                        [handlers = std::move(handlers), workspaceId, uiLog = m_uiLog]() {
                            if (auto log = uiLog.lock())
                            {
                                auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
                                log->WriteLog(node->QueryValue(ORTHIA_TCSTR("analysis-complete")));
                            }
                        });
                });
            }

            {
                auto symOp = std::make_shared<oui::BaseOperation>(uiThread);
                m_analyzer.EnqueueLoadSymbols(workspaceId, info, symOp,
                    [this, uiThread, workspaceId, info, mainAddr]() {
                        auto reanalyzeOp = std::make_shared<oui::BaseOperation>(uiThread);
                        m_analyzer.EnqueueAnalyzePrivateSymbols(workspaceId, info, reanalyzeOp, mainAddr,
                            [this, uiThread, workspaceId]() {
                                std::vector<std::shared_ptr<IUIEventHandler>> handlers;
                                {
                                    std::unique_lock<std::mutex> lock(m_lock);
                                    handlers.assign(m_handlers.begin(), m_handlers.end());
                                }
                                uiThread->AddTask(
                                    [handlers = std::move(handlers), workspaceId]() {
                                        for (auto& h : handlers)
                                            h->OnWorkspaceItemChanged(workspaceId);
                                    });
                            });
                    });
            }
    }
    void CProgramModel::AddExecutable(std::shared_ptr<oui::IFile2> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler)
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

            // detect format and map
            const int executableType = DianaExecutable_DetectType(binPeFile.data(), binPeFile.size());
            orthia::MapFileParameters params;
            auto mappedExe = orthia::MakeSimpleFile(executableType, binPeFile, params);

            // check folder
            auto fileHash = CalcSha1(binPeFile);
            auto fileHashStr = orthia::ToHexString(fileHash.data(), fileHash.size());
            auto dbFolder = m_config->GetDBFolder() + AddSlash2(fileHashStr);
            auto dbFileName = dbFolder + m_config->GetDBFileName();
            auto binFileName = dbFolder + m_config->GetBinFileName();
            auto readmeFileName = dbFolder + m_config->GetReadmeFileName();
            CreateAllDirectoriesForFile(dbFileName);
            WriteLog(completeHandler->GetThread(), oui::PassParameter1(mainNode->QueryValue(ORTHIA_TCSTR("database-file")),
                dbFileName));

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
                    result.error = errorNode->QueryValue(ORTHIA_TCSTR("too-big"));
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

            info->fullName = file->GetFullFileName();
            info->file = mappedExe;
            {
                oui::String shortName;
                orthia::UnparseFileNameFromFullFileName(info->fullName.native, &shortName.native);
                info->shortName = std::move(shortName);
            }
            info->moduleManager = std::make_shared<CModuleManager>();
            info->moduleManager->Reinit(dbFileName, false);
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

            bool firstOpen = false;
            if (!info->moduleManager->QueryDatabaseManager()->GetClassicDatabase()->IsModuleExists(info->file->GetImageBase()))
            {
                firstOpen = true;
                // first open, warn user it may take quite a time
                WriteLog(completeHandler->GetThread(), mainNode->QueryValue(ORTHIA_TCSTR("analyzing-file")));
            }

            if (firstOpen)
            {
                {
                    orthia::CImportsLoader importsLoader(executableType, completeHandler);
                    importsLoader.LoadModules(file->GetFullFileName(), mappedExe, file->GetFileSystem());

                    info->moduleManager->ReloadModule(info->file->GetImageBase(),
                        &reader,
                        false,
                        info->shortName.native,
                        0);

                    importsLoader.ReportModules(info->moduleManager);
                }

                InsertModuleMetaInfo(info->moduleManager->QueryDatabaseManager()->GetClassicDatabase(),
                    info->file->GetImageBase(),
                    info->fullName.native);
            }

            result.extraInfo[model_OpenResult_extraInfo_WorkspaceId] = std::any(RegisterItem(info, false));
            // OK
            result.error.native.clear();
        }
        catch (std::exception& e)
        {
            result.error.native = orthia::Utf8ToPlatformString(e.what());
        }
    }
}