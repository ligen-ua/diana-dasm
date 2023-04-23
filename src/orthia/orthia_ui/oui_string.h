#pragma once

#include "oui_base.h"


namespace oui

{
    struct String
    {
#ifdef OUI_SYS_WINDOWS
        std::wstring native;
#define OUI_TCSTR(X) L##X
#define OUI_STR(X) std::wstring(OUI_TCSTR(X))
#define OUI_TO_STR(X) std::to_wstring(X)


#else
        std::string native;
#define OUI_TCSTR(X) X
#define OUI_STR(X) std::string(X)
#define OUI_TO_STR(X) std::to_string(X)

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