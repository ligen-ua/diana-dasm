#include "oui_base.h"

namespace oui
{

    static bool g_consoleSupportsUTF16 = false;
    static int CalculateSymbolsCountUTF16(const wchar_t* pStart, size_t sizeInWchars, const wchar_t exceptSym_in)
    {
        wchar_t exceptSym = exceptSym_in;
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            wchar_t ch = *p;
            if (ch == exceptSym)
            {
                exceptSym = 0;
                continue;
            }
            ++charCount;
            if (IsLeadByte(ch))
            {
                ++p;
                if (p >= pEnd)
                {
                    break;
                }
            }
            exceptSym = exceptSym_in;
        }
        return charCount;
    }

    static int CalculateSymbolsCountUCS2(const wchar_t* pStart, size_t sizeInWchars, const wchar_t exceptSym_in)
    {
        if (exceptSym_in == 0)
        {
            return (int)sizeInWchars;
        }
        wchar_t exceptSym = exceptSym_in;
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            wchar_t ch = *p;
            if (ch == exceptSym)
            {
                exceptSym = 0;
                continue;
            }
            ++charCount;
            exceptSym = exceptSym_in;
        }
        return charCount;
    }

    int CalculateSymbolsCount(const wchar_t* pStart, size_t sizeInWchars, const wchar_t exceptSym_in)
    {
        if (g_consoleSupportsUTF16)
        {
            return CalculateSymbolsCountUTF16(pStart, sizeInWchars, exceptSym_in);
        }
        return CalculateSymbolsCountUCS2(pStart, sizeInWchars, exceptSym_in);
    }
}