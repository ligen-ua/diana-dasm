#pragma once

#include "orthia_utils.h"

namespace orthia
{
    class CConfigOptionsStorage
    {
        PlatformString_type m_appDir;
        PlatformString_type m_dbDir;
        PlatformString_type m_binDir;
        PlatformString_type m_procDBDir;

        std::vector<PlatformString_type> m_symbolFolders;

    public:
        void Init();
        PlatformString_type GetBinFileName() const;
        PlatformString_type GetDBFileName() const;
        PlatformString_type GetDBFolder() const;
        PlatformString_type GetReadmeFileName() const;
        PlatformString_type GetBinFolder() const;
        PlatformString_type GetProcDBFolder() const;
        std::vector<PlatformString_type> GetSymbolsFolders() const;
        void SetSymbolsFolders(const PlatformString_type& names);
    };
}