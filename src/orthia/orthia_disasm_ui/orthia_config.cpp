#include "orthia_config.h"
#include "orthia_model.h"

namespace orthia
{
    const PlatformString_type g_rootFolderName = ORTHIA_TCSTR("Orthia");
    const PlatformString_type g_nextDB = ORTHIA_TCSTR("db");
    const PlatformString_type g_binFolder = ORTHIA_TCSTR("bin");
    const PlatformString_type g_nextProc = ORTHIA_TCSTR("proc");
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
        m_procDBDir = m_appDir + AddSlash2(g_nextProc);
        m_binDir = m_appDir + AddSlash2(g_binFolder);

        orthia::CreateAllDirectoriesForFile(m_dbDir);
        orthia::CreateAllDirectoriesForFile(m_binDir);
        orthia::CreateAllDirectoriesForFile(m_procDBDir);
    }
    PlatformString_type CConfigOptionsStorage::GetReadmeFileName() const
    {
        // haha, this is a joke for us, u know, the old people
        return ORTHIA_TCSTR("DIRINFO");
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
    PlatformString_type CConfigOptionsStorage::GetProcDBFolder() const
    {
        return m_procDBDir;
    }
    PlatformString_type CConfigOptionsStorage::GetBinFolder() const
    {
        return m_binDir;
    }
    std::vector<PlatformString_type> CConfigOptionsStorage::GetSymbolsFolders() const
    {
        std::vector<PlatformString_type> res;
#ifdef DIANA_HAS_WIN32
        res.push_back(L"C:\\Sym");
        res.push_back(L"C:\\Symbols");
#else
        res.push_back("~/sym");
        res.push_back("~/symbols");
#endif
        return res;
    }
}
