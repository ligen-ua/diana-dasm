#include "orthia_model.h"

namespace orthia
{
    CProgramModel::CProgramModel()
    {
        m_fileSystem = std::make_shared<oui::CFileSystem>();
    }

    std::shared_ptr<oui::CFileSystem>  CProgramModel::GetFileSystem()
    {
        return m_fileSystem;
    }
    void CProgramModel::QueryWorkspace(std::vector<std::string>& allNames, std::string& active)
    {
    }

}