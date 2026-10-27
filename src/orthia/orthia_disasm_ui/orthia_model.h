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

        void WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line);
        void CreateProcItemFS(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            intrusive_ptr<CTextNode> mainNode,
            intrusive_ptr<CTextNode> errorNode,
            std::shared_ptr<CProcessWorkplaceItem> info);

        int RegisterItem(std::shared_ptr<IWorkPlaceItem> item, bool makeActive);
        // the end of an open of a process item, live or saved: registered, logged, analysis queued
        void FinishProcessOpen(std::shared_ptr<CProcessWorkplaceItem> info,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            intrusive_ptr<CTextNode> mainNode,
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

        // other thread
        void AddProcess(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler);
        // a process folder after the process exited (OpenSavedProcess): its database as it is,
        // the bytes from the module files and the floating code snapshots
        void AddSavedProcess(std::shared_ptr<CSavedProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler);
        // a --proc-folder argument: a folder, its data.db, a pid or a folder name of the data
        // folder. Throws with a message for the user
        std::shared_ptr<CSavedProcess> OpenSavedProcessFolder(const PlatformString_type& argument);

        // forcedFormat: open as this format instead of the detected one; params: what a
        // format opened by hand needs (raw code: base and mode)
        void AddExecutable(std::shared_ptr<oui::IFile2> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            const IImageFormat* forcedFormat = nullptr,
            MapFileParameters params = MapFileParameters());

        void AsyncOpenFileWithType(oui::ThreadPtr_type thread,
            const oui::FileUnifiedId& fileId,
            oui::FileWithTypeRecipientHandler_type resultCallback);

        CModuleAnalyzer& GetAnalyzer() { return m_analyzer; }
        std::shared_ptr<orthia::CConfigOptionsStorage> GetConfig() { return m_config; }

    private:
        CModuleAnalyzer m_analyzer;

        // Dispatches data-refreshed event to all UI subscribers from a background thread.
        void NotifyWorkspaceDataRefreshed(std::shared_ptr<oui::CWindowThread> uiThread, int workspaceId);

        // Starts background analysis and symbol-loading pipelines for a newly opened workspace item.
        // procItem is non-null for process items; file items pass nullptr to skip process-only steps
        // (disassembly analysis and private-symbol re-analysis).
        void EnqueueAnalysisOps(std::shared_ptr<oui::CWindowThread> uiThread,
            int workspaceId,
            std::shared_ptr<IWorkPlaceItem> item,
            Address_type mainAddr,
            std::shared_ptr<CProcessWorkplaceItem> procItem);
    };
    oui::String ReadFileToVector(std::shared_ptr<oui::IFile> file, std::vector<char>& data, std::shared_ptr<oui::BaseOperation> operation = nullptr, intrusive_ptr<CTextNode> errorNode = nullptr);

}