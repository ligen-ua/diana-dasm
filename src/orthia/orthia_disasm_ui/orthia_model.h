#pragma once

#include "orthia_utils.h"
#include "oui_filesystem.h"
#include "orthia_text_manager.h"
#include "orthia_config.h"
#include "orthia_module_manager.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

namespace orthia
{
    class CSimplePeFile;
    class CModuleManager;
    struct IWorkPlaceItem
    {
        virtual ~IWorkPlaceItem() {}
        virtual const orthia::CSimplePeFile* GetFile() const = 0;
        virtual const std::shared_ptr<CModuleManager> GetModuleManager() const= 0;
    };
    class CSimplePeFile;
    struct WorkplaceItemInternal:IWorkPlaceItem
    {
        std::unique_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
        std::shared_ptr<CModuleManager> moduleManager;

        // public interface
        const orthia::CSimplePeFile* GetFile() const override;
        const std::shared_ptr<CModuleManager> GetModuleManager() const override;
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
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;

        mutable std::mutex m_lock;
        std::map<int, std::shared_ptr<WorkplaceItemInternal>> m_items;
        int m_lastUid = 0;

        int m_activeId = 0;
        std::shared_ptr<orthia::CConfigOptionsStorage> m_config;
        std::weak_ptr<IUILogInterface> m_uiLog;

        void WriteLog(std::shared_ptr<oui::CWindowThread> thread, const oui::String& line);
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;
        void SetUILog(std::shared_ptr<IUILogInterface> uiLog);
        std::shared_ptr<IWorkPlaceItem> GetItem(int uid);

        // other thread
        void AddExecutable(std::shared_ptr<oui::IFile> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            bool makeActive);

    };
}