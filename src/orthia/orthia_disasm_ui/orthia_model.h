#pragma once

#include "orthia_model_interfaces.h"
#include "oui_filesystem.h"
#include "orthia_config.h"
#include "orthia_module_manager.h"
#include "oui_processes.h"
#include <optional>
#include "orthia_commands.h"
#include "orthia_module_analyzer.h"
#include "orthia_simple_file.h"

namespace orthia
{
    class CSimplePeFile;
    class CModuleManager;
    struct IImageFormat;
    struct WorkplaceItem
    {
        int uid = 0;
        oui::String name;
    };
    // OpenResult extra fields
    const int model_OpenResult_extraInfo_InitalAddress = 1;
    const int model_OpenResult_extraInfo_WorkspaceId = 2;
    // bool, true: the database is new and nothing was started (AutoTasksPolicy::AskIfNew),
    // the UI asks what to start and calls StartAutoTasks
    const int model_OpenResult_extraInfo_AskAutoTasks = 3;

    struct FolderMeta;
    // what to start after an open. isNewDb: the database was made by this open; meta: the
    // meta.xml of an earlier one, or nullptr. *ask: the UI has to ask (nothing is started)
    AutoTasks ResolveAutoTasks(const OpenOptions& options, bool isNewDb, const FolderMeta* meta, bool* ask);

    struct IUIEventHandler
    {
        virtual ~IUIEventHandler() {}
        virtual void OnPreWorkspaceItemChange(int itemId) = 0;
        virtual void OnWorkspaceItemChanged(int itemId) = 0;
        virtual void OnWorkspaceDataRefreshed(int itemId) = 0;
        virtual void OnWorkspaceItemRemoved(int itemId) {}
    };
    class CProcessWorkplaceItem;
    class CSavedProcess;
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;
        std::shared_ptr<oui::CProcessSystem> m_processSystem;
        std::shared_ptr<orthia::CCommandProcessor> m_commandProcessor;

        mutable std::mutex m_lock;
        std::map<int, std::shared_ptr<IWorkPlaceItem>> m_items;
        int m_lastUid = 0;

        int m_activeId = 0;
        std::shared_ptr<orthia::CConfigOptionsStorage> m_config;
        std::weak_ptr<IUILogInterface> m_uiLog;

        std::set<std::shared_ptr<IUIEventHandler>> m_handlers;

        // the items opened with a new database, waiting for StartAutoTasks
        struct PendingAutoTasks
        {
            std::shared_ptr<oui::CWindowThread> uiThread;
            Address_type mainAddr = 0;
            bool isProcess = false;
        };
        std::map<int, PendingAutoTasks> m_pendingAutoTasks;

        void WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line);
        // false: no database of an earlier open is reused (a new one, or it failed). The meta.xml
        // of the reused one goes to savedMeta
        bool CreateProcItemFS(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            intrusive_ptr<CTextNode> mainNode,
            intrusive_ptr<CTextNode> errorNode,
            std::shared_ptr<CProcessWorkplaceItem> info,
            FolderMeta& savedMeta);

        int RegisterItem(std::shared_ptr<IWorkPlaceItem> item, bool makeActive);
        // the end of an open of a process item, live or saved: registered, logged, analysis queued
        void FinishProcessOpen(std::shared_ptr<CProcessWorkplaceItem> info,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            intrusive_ptr<CTextNode> mainNode,
            oui::fsui::OpenResult& result,
            const OpenOptions& options,
            bool isNewDb,
            const FolderMeta* savedMeta);
        // after an open: queues the tasks, or leaves them to StartAutoTasks and tells the UI to ask
        void StartOrAskAutoTasks(std::shared_ptr<oui::CWindowThread> uiThread,
            int workspaceId,
            std::shared_ptr<IWorkPlaceItem> item,
            Address_type mainAddr,
            bool isProcess,
            const OpenOptions& options,
            bool isNewDb,
            const FolderMeta* savedMeta,
            oui::fsui::OpenResult& result);
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);
        void Stop();

        void SubscribeUI(std::shared_ptr<IUIEventHandler> handler);
        void UnsubscribeUI(std::shared_ptr<IUIEventHandler> handler);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        std::shared_ptr<oui::CProcessSystem> GetProcessSystem();
        std::shared_ptr<orthia::CCommandProcessor> GetCommandProcessor();

        int GetActiveItemId() const { return m_activeId; }
        bool QueryWorkspaceItem(int id, WorkplaceItem& item) const;
        bool SetActiveItem(int uid);
        bool RemoveItem(int uid);
        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        // database folder -> short name of the item that has it open
        std::map<PlatformString_type, PlatformString_type> QueryOpenDatabaseFolders() const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;
        void SetUILog(std::shared_ptr<IUILogInterface> uiLog);
        std::shared_ptr<IWorkPlaceItem> GetItem(int uid);
        std::shared_ptr<IWorkPlaceItem> GetActiveItem();

        // other thread. options: what starts after the open (analysis, symbols)
        void AddProcess(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            const OpenOptions& options);
        // a process folder after the process exited (OpenSavedProcess): its database as it is,
        // the bytes from the module files and the floating code snapshots
        void AddSavedProcess(std::shared_ptr<CSavedProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            const OpenOptions& options);
        // a --proc-folder argument: a folder, its data.db, a pid or a folder name of the data
        // folder. Throws with a message for the user
        std::shared_ptr<CSavedProcess> OpenSavedProcessFolder(const PlatformString_type& argument);

        // forcedFormat: open as this format instead of the detected one; params: what a
        // format opened by hand needs (raw code: base and mode)
        void AddExecutable(std::shared_ptr<oui::IFile2> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            const OpenOptions& options,
            const IImageFormat* forcedFormat = nullptr,
            MapFileParameters params = MapFileParameters());

        // UI thread: the answer to model_OpenResult_extraInfo_AskAutoTasks. Saved in meta.xml
        // for the next opens, then queued. Nothing happens when the item is gone or was not asked
        void StartAutoTasks(int workspaceId, const AutoTasks& tasks);

        void AsyncOpenFileWithType(oui::ThreadPtr_type thread,
            const oui::FileUnifiedId& fileId,
            oui::FileWithTypeRecipientHandler_type resultCallback);

        CModuleAnalyzer& GetAnalyzer() { return m_analyzer; }
        std::shared_ptr<orthia::CConfigOptionsStorage> GetConfig() { return m_config; }

    private:
        CModuleAnalyzer m_analyzer;

        // Dispatches data-refreshed event to all UI subscribers from a background thread.
        void NotifyWorkspaceDataRefreshed(std::shared_ptr<oui::CWindowThread> uiThread, int workspaceId);

        // Starts the background tasks for a newly opened workspace item: the analysis of the main
        // module, the symbols (of all modules for a process, of the main one for a file), then
        // the analysis again with the private symbols when both are on.
        void EnqueueAnalysisOps(std::shared_ptr<oui::CWindowThread> uiThread,
            int workspaceId,
            std::shared_ptr<IWorkPlaceItem> item,
            Address_type mainAddr,
            bool isProcess,
            const AutoTasks& tasks);
    };
    oui::String ReadFileToVector(std::shared_ptr<oui::IFile> file, std::vector<char>& data, std::shared_ptr<oui::BaseOperation> operation = nullptr, intrusive_ptr<CTextNode> errorNode = nullptr);

}