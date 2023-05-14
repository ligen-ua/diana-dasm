#pragma once

#include "orthia_utils.h"
#include "oui_filesystem.h"
#include "orthia_text_manager.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

namespace orthia
{
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;

    public:
        CProgramModel();

        std::shared_ptr<oui::CFileSystem> GetFileSystem();
        void QueryWorkspace(std::vector<std::string>& allNames, std::string& active);

        // other thread
        void AddExecutable(std::shared_ptr<oui::IFile> file,
            oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler);

    };
}