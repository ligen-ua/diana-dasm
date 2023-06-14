#pragma once

#include "orthia_utils.h"

namespace orthia
{
    class CConfigOptionsStorage
    {
        PlatformString_type m_appDir;
        PlatformString_type m_dbDir;

    public:
        void Init();
        PlatformString_type GetBinFileName() const;
        PlatformString_type GetDBFileName() const;
        PlatformString_type GetDBFolder() const;
    };
}