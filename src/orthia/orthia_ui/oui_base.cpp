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

    static int CalculateSymbolsCountUTF16(const wchar_t* pStart, size_t sizeInWchars, std::vector<SymbolInfo>& symbols)
    {
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            SymbolInfo info;
            info.charOffset = (int)(p - pStart);
            info.sizeInTChars = 1;

            symbols.push_back(info);
            wchar_t ch = *p;
            ++charCount;
            if (IsLeadByte(ch))
            {
                ++info.sizeInTChars;
                ++p;
                if (p >= pEnd)
                {
                    break;
                }
            }
        }
        return charCount;
    }

    int CalculateSymbolsCount(const wchar_t* pStart,
        size_t sizeInWchars,
        std::vector<SymbolInfo>& symbols)
    {
        symbols.clear();
        if (g_consoleSupportsUTF16)
        {
            return CalculateSymbolsCountUTF16(pStart, sizeInWchars, symbols);
        }
        symbols.resize(sizeInWchars, SymbolInfo());
        int pos = 0;
        for (auto& sym : symbols)
        {
            sym.charOffset = pos;
            sym.sizeInTChars = 1;
            ++pos;
        }
        return sizeInWchars;
    }

    // CutString
    static int CutStringUTF16(std::wstring& str, int maxCharsCount)
    {
        int charCount = 0;
        const wchar_t* pEnd = str.c_str() + str.size();
        for (const wchar_t* p = str.c_str(); p < pEnd; ++p)
        {
            wchar_t ch = *p;
            if (charCount >= maxCharsCount)
            {
                str.resize(p - str.c_str());
                break;
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
        }
        return charCount;
    }
    int CutString(std::wstring& str, int maxCharsCount)
    {
        if (g_consoleSupportsUTF16)
        {
            return CutStringUTF16(str, maxCharsCount);
        }
        if (str.size() > maxCharsCount)
        {
            str.resize(maxCharsCount);
            return maxCharsCount;
        }
        return (int)str.size();
    }

    // other
    bool IsInside(const Range& range, int value)
    {
        return value >= range.begin && value < range.end;
    }
    bool IsInside(const Rect& rect, Point& pt)
    {
        if (pt.x < rect.position.x)
            return false;
        if (pt.y < rect.position.y)
            return false;
        int xend = rect.position.x + rect.size.width;
        int yend = rect.position.y + rect.size.height;
        if (pt.x >= xend)
        {
            return false;
        }
        if (pt.y >= yend)
        {
            return false;
        }
        return true;
    }

}