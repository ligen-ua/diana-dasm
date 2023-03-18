#pragma once

#include "oui_base.h"

namespace oui
{
    struct String
    {
#ifdef _WIN32
        std::wstring native;
#else
        std::string native;
#endif
    };
}