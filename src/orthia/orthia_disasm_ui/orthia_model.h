#pragma once

#include "orthia_utils.h"
#include "oui_filesystem.h"

namespace orthia
{
    class CProgramModel
    {
        std::shared_ptr<oui::CFileSystem> m_fileSystem;

    public:
        CProgramModel();

        std::shared_ptr<oui::CFileSystem> GetFileSystem();

        void QueryWorkspace(std::vector<std::string>& allNames, std::string& active);
    };
}