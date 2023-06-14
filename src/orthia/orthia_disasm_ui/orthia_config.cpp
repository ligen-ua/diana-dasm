#include "orthia_config.h"
#include "orthia_model.h"

namespace orthia
{
    const PlatformString_type g_rootFolderName = ORTHIA_TCSTR("Orthia");
    const PlatformString_type g_nextDB = ORTHIA_TCSTR("db");

    void CConfigOptionsStorage::Init()
    {
        auto errorNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        PlatformString_type appDataFolder;
        int error = GetAppDataFolderWithSlash_Silent(appDataFolder);
        if (error)
        {
            auto text = errorNode->QueryValue(ORTHIA_TCSTR("cant-open-file"));
            throw orthia::CWin32Exception(PlatformStringToUtf8(text), error);
        }

        m_appDir = appDataFolder + AddSlash2(g_rootFolderName);
        m_dbDir = m_appDir + AddSlash2(g_nextDB);

        orthia::CreateAllDirectoriesForFile(m_dbDir);
    }

    PlatformString_type CConfigOptionsStorage::GetBinFileName() const
    {
        return ORTHIA_TCSTR("target.bin");
    }
    PlatformString_type CConfigOptionsStorage::GetDBFileName() const
    {
        return ORTHIA_TCSTR("data.db");
    }

    PlatformString_type CConfigOptionsStorage::GetDBFolder() const
    {
        return m_dbDir;
    }

}
