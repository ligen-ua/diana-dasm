#include "oui_string.h"

namespace oui
{
    oui::String PassParameter1(const oui::String& text, const oui::String& param1)
    {
        String::StringStream_type res;
        for (int i = 0; i < (int)text.native.size(); ++i)
        {
            if (text.native[i] == OUI_TCHAR('%') && i + 1 < (int)text.native.size() && text.native[i + 1] == OUI_TCHAR('1'))
            {
                res << param1.native;
                ++i;
                continue;
            }
            res << text.native[i];
        }
        return res.str();
    }
    oui::String PassParameter2(const oui::String& text,
        const oui::String& param1,
        const oui::String& param2)
    {
        String::StringStream_type res;
        for (int i = 0; i < (int)text.native.size(); ++i)
        {
            if (text.native[i] == OUI_TCHAR('%') && i + 1 < (int)text.native.size())
            {
                switch (text.native[i + 1])
                {
                case OUI_TCHAR('1'):
                    res << param1.native;
                    break;
                case OUI_TCHAR('2'):
                    res << param2.native;
                    break;
                default:
                    continue;
                }
                ++i;
                continue;
            }
            res << text.native[i];
        }
        return res.str();
    }

    oui::String PassParameter3(const oui::String& text,
        const oui::String& param1,
        const oui::String& param2,
        const oui::String& param3)
    {
        String::StringStream_type res;
        for (int i = 0; i < (int)text.native.size(); ++i)
        {
            if (text.native[i] == OUI_TCHAR('%') && i + 1 < (int)text.native.size())
            {
                switch (text.native[i + 1])
                {
                case OUI_TCHAR('1'):
                    res << param1.native;
                    break;
                case OUI_TCHAR('2'):
                    res << param2.native;
                    break;
                case OUI_TCHAR('3'):
                    res << param3.native;
                    break;
                default:
                    continue;
                }
                ++i;
                continue;
            }
            res << text.native[i];
        }
        return res.str();
    }
}