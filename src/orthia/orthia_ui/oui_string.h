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
        typedef typename decltype(native)::value_type char_type;
        typedef decltype(native) string_type;
        const static char_type symSpace = (char_type)' ';

        String()
        {
        }
        String(const string_type& str)
            :
            native(str)
        {
        }
        String(const char_type * p)
            :
            native(p)
        {
        }
    };
}