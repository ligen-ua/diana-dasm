#include "orthia_config.h"
#include "orthia_model.h"
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

static void CleanupOldProcFolders(const orthia::PlatformString_type& procFolderWithSlash)
{
    const auto threshold = std::chrono::hours(48);
    const auto now = fs::file_time_type::clock::now();

    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fs::path(procFolderWithSlash), ec))
    {
        std::error_code ec2;
        if (!entry.is_directory(ec2))
            continue;

        auto newestTime = fs::file_time_type::min();
        std::error_code ec3;
        for (const auto& fileEntry : fs::directory_iterator(entry.path(), ec3))
        {
            std::error_code ec4;
            auto wt = fs::last_write_time(fileEntry, ec4);
            if (!ec4 && wt > newestTime)
                newestTime = wt;
        }

        if (newestTime == fs::file_time_type::min())
        {
            newestTime = fs::last_write_time(entry, ec2);
            if (ec2)
                continue;
        }

        if (newestTime < now && ((now - newestTime) > threshold))
        {
            std::error_code ec5;
            fs::remove_all(entry.path(), ec5);
        }
    }
}

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
        CleanupOldProcFolders(m_procDBDir);

#ifdef DIANA_HAS_WIN32
        m_symbolFolders.push_back(L"C:\\Sym");
        m_symbolFolders.push_back(L"C:\\Symbols");
#else
        m_symbolFolders.push_back("~/sym");
        m_symbolFolders.push_back("~/symbols");
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
    PlatformString_type CConfigOptionsStorage::GetBinFolder() const
    {
        return m_binDir;
    }

    void CConfigOptionsStorage::SetSymbolsFolders(const PlatformString_type& names)
    {
        orthia::SplitStringWithoutWhitespace(names, orthia::StringInfo(ORTHIA_TCSTR(";")), &m_symbolFolders);
    }
    std::vector<PlatformString_type> CConfigOptionsStorage::GetSymbolsFolders() const
    {
        return m_symbolFolders;
    }
}
