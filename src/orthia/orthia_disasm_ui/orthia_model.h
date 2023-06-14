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
    struct WorkplaceItemInternal
    {
        std::unique_ptr<orthia::CSimplePeFile> peFile;
        oui::String fullName, shortName;
    };

    struct WorkplaceItem
    {
        int uid = 0;
        oui::String name;
    };
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;

        mutable std::mutex m_lock;
        std::map<int, std::shared_ptr<WorkplaceItemInternal>> m_items;
        int m_lastUid = 0;

        int m_activeId = 0;
        std::shared_ptr<orthia::CConfigOptionsStorage> m_config;
    public:
        CProgramModel(std::shared_ptr<orthia::CConfigOptionsStorage> config);

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        int QueryWorkspaceItems(std::vector<WorkplaceItem>& items) const;
        bool QueryActiveWorkspaceItem(WorkplaceItem& item) const;

        // other thread
        void AddExecutable(std::shared_ptr<oui::IFile> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
            bool makeActive);

    };
}