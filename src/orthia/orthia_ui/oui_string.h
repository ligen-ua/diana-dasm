#pragma once

#include "oui_base.h"

namespace oui
{
    struct String
    {
        std::wstring native;
        std::string utf8;
    };
}