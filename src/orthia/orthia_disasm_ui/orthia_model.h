#pragma once

#include "orthia_model_interfaces.h"
#include "oui_filesystem.h"
#include "orthia_text_manager.h"
#include "orthia_config.h"
#include "orthia_module_manager.h"
#include "oui_processes.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

namespace orthia
{
    class CSimplePeFile;
    class CModuleManager;

    struct FileWorkplaceItem:std::enable_shared_from_this<FileWorkplaceItem>, IWorkPlaceItem
    {
        std::unique_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;
        Address_type moduleLastValidAddress = 0;

        // public interface
        WorkAddressData ReadData(Address_type address, Address_type size) override;
        WorkAddressRangeInfo GetRangeInfo(Address_type address) const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
        oui::String GetShortName() const override;
        void ReloadModules() override;
        void GetModules(std::vector<orthia::ModuleInfo>& modules) const override;
        int GetModulesCount() const override;
    };

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

    struct IUIEventHandler
    {
        virtual ~IUIEventHandler() {}
        virtual void OnPreWorkspaceItemChange(int itemId) = 0;
        virtual void OnWorkspaceItemChanged(int itemId) = 0;
    };
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;
        std::shared_ptr<oui::CProcessSystem> m_processSystem;

        mutable std::mutex m_lock;
        std::map<int, std::shared_ptr<IWorkPlaceItem>> m_items;
        int m_lastUid = 0;

        int m_activeId = 0;
        std::shared_ptr<orthia::CConfigOptionsStorage> m_config;
        std::weak_ptr<IUILogInterface> m_uiLog;

        std::set<std::shared_ptr<IUIEventHandler>> m_handlers;

        void WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line);

        void RegisterItem(std::shared_ptr<IWorkPlaceItem> item, bool makeActive);
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);

        void SubscribeUI(std::shared_ptr<IUIEventHandler> handler);
        void UnsubscribeUI(std::shared_ptr<IUIEventHandler> handler);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        std::shared_ptr<oui::CProcessSystem> GetProcessSystem();

        bool SetActiveItem(int uid);
        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;
        void SetUILog(std::shared_ptr<IUILogInterface> uiLog);
        std::shared_ptr<IWorkPlaceItem> GetItem(int uid);
        std::shared_ptr<IWorkPlaceItem> GetActiveItem();

        // other thread
        void AddProcess(std::shared_ptr<oui::IProcess> proc,
            oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler,
            bool makeActive);

        void AddExecutable(std::shared_ptr<oui::IFile> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            bool makeActive);

    };
}