#include "orthia_config.h"
#include "orthia_model.h"
#include "orthia_databases.h"
#include <stdlib.h>

namespace orthia
{
    const PlatformString_type g_rootFolderName = ORTHIA_TCSTR("Orthia");
    const PlatformString_type g_nextDB = ORTHIA_TCSTR("db");
    const PlatformString_type g_binFolder = ORTHIA_TCSTR("bin");
    const PlatformString_type g_nextProc = ORTHIA_TCSTR("proc");
    static PlatformString_type QueryEnvironmentString(const PlatformString_type::value_type* name)
    {
#ifdef DIANA_HAS_WIN32
        wchar_t* value = nullptr;
        size_t size = 0;
        if (_wdupenv_s(&value, &size, name) || !value)
        {
            return PlatformString_type();
        }
        PlatformString_type text(value);
        free(value);
        return text;
#else
        const char* value = getenv(name);
        return PlatformString_type(value ? value : "");
#endif
    }
    void CConfigOptionsStorage::Init()
    {
        // ORTHIA_HOME replaces the whole <app data>/Orthia folder (used by tests to isolate the databases)
        auto orthiaHome = QueryEnvironmentString(ORTHIA_TCSTR("ORTHIA_HOME"));
        if (!orthiaHome.empty())
        {
            m_appDir = AddSlash2(orthiaHome);
        }
        else
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
        }
        m_dbDir = m_appDir + AddSlash2(g_nextDB);
        m_procDBDir = m_appDir + AddSlash2(g_nextProc);
        m_binDir = m_appDir + AddSlash2(g_binFolder);

        orthia::CreateAllDirectoriesForFile(m_dbDir);
        orthia::CreateAllDirectoriesForFile(m_binDir);
        orthia::CreateAllDirectoriesForFile(m_procDBDir);
        CleanupExpiredProcFolders(m_procDBDir);

        // ORTHIA_SYMBOL_PATH replaces the default symbol folders, same format as .symfix
        auto symbolPath = QueryEnvironmentString(ORTHIA_TCSTR("ORTHIA_SYMBOL_PATH"));
        if (!symbolPath.empty())
        {
            SetSymbolsFolders(symbolPath);
            return;
        }
#ifdef DIANA_HAS_WIN32
        m_symbolFolders.push_back(L"C:\\Sym");
        m_symbolFolders.push_back(L"C:\\Symbols");
#else
        m_symbolFolders.push_back(ExpandHomeFolder("~/sym", QueryEnvironmentString("HOME")));
        m_symbolFolders.push_back(ExpandHomeFolder("~/symbols", QueryEnvironmentString("HOME")));
#endif
    }
    PlatformString_type CConfigOptionsStorage::GetReadmeFileName() const
    {
        // haha, this is a joke for us, u know, the old people
        return ORTHIA_TCSTR("DIRINFO");
    }
    PlatformString_type CConfigOptionsStorage::GetParamsFileName() const
    {
        return ORTHIA_TCSTR("parameters.xml");
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
    PlatformString_type CConfigOptionsStorage::GetDataFolder() const
    {
        return m_appDir;
    }
    PlatformString_type CConfigOptionsStorage::GetBinFolder() const
    {
        return m_binDir;
    }

    PlatformString_type ExpandHomeFolder(const PlatformString_type& path, const PlatformString_type& home)
    {
#ifdef DIANA_HAS_WIN32
        (void)home;
        return path;
#else
        // only "~" and "~/...": "~user" would need a passwd lookup
        if (home.empty() || path.empty() || path[0] != '~' || (path.size() > 1 && path[1] != '/'))
        {
            return path;
        }
        PlatformString_type result = home;
        while (!result.empty() && result.back() == '/')
        {
            result.pop_back();
        }
        if (path.size() == 1)
        {
            return result.empty() ? PlatformString_type("/") : result;
        }
        return result + path.substr(1);
#endif
    }
    void CConfigOptionsStorage::SetSymbolsFolders(const PlatformString_type& names)
    {
        orthia::SplitStringWithoutWhitespace(names, orthia::StringInfo(ORTHIA_TCSTR(";")), &m_symbolFolders);
        const auto home = QueryEnvironmentString(ORTHIA_TCSTR("HOME"));
        for (auto& folder : m_symbolFolders)
        {
            folder = ExpandHomeFolder(folder, home);
        }
    }
    std::vector<PlatformString_type> CConfigOptionsStorage::GetSymbolsFolders() const
    {
        return m_symbolFolders;
    }
}
