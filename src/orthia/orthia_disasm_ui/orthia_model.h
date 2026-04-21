#pragma once

#include "orthia_model_interfaces.h"
#include "oui_filesystem.h"
#include "orthia_text_manager.h"
#include "orthia_config.h"
#include "orthia_module_manager.h"
#include "oui_processes.h"
#include <optional>
#include "orthia_commands.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

namespace orthia
{
    class CSimplePeFile;
    class CModuleManager;
    struct WorkplaceItem
    {
        int uid = 0;
        oui::String name;
    };
    struct IUILogInterface
    {
        virtual ~IUILogInterface() {}
        virtual void WriteLog(const oui::String& line) = 0;
    };

    // OpenResult extra fields
    const int model_OpenResult_extraInfo_InitalAddress = 1;
    const int model_OpenResult_extraInfo_WorkspaceId = 2;

    struct IUIEventHandler
    {
        virtual ~IUIEventHandler() {}
        virtual void OnPreWorkspaceItemChange(int itemId) = 0;
        virtual void OnWorkspaceItemChanged(int itemId) = 0;
    };
    class CProcessWorkplaceItem;
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
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);

        void SubscribeUI(std::shared_ptr<IUIEventHandler> handler);
        void UnsubscribeUI(std::shared_ptr<IUIEventHandler> handler);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        std::shared_ptr<oui::CProcessSystem> GetProcessSystem();
        std::shared_ptr<orthia::CCommandProcessor> GetCommandProcessor();

        int GetActiveItemId() const { return m_activeId; }
        bool QueryWorkspaceItem(int id, WorkplaceItem& item) const;
        bool SetActiveItem(int uid);
        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;
        void SetUILog(std::shared_ptr<IUILogInterface> uiLog);
        std::shared_ptr<IWorkPlaceItem> GetItem(int uid);
        std::shared_ptr<IWorkPlaceItem> GetActiveItem();

        // other thread
        void AddProcess(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler);

        void AddExecutable(std::shared_ptr<oui::IFile2> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler);

        void LoadSymbols(std::shared_ptr<IWorkPlaceItem> workItem,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler);
    };
    oui::String ReadFileToVector(std::shared_ptr<oui::IFile> file, std::vector<char>& data, std::shared_ptr<oui::BaseOperation> operation = nullptr, intrusive_ptr<CTextNode> errorNode = nullptr);

}