#include "oui_symbols_win32.h"

namespace oui
{

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

    static int CalculateSymbolsCountUTF16(const wchar_t* pStart, size_t sizeInWchars, std::vector<SymbolInfo>& symbols)
    {
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            {
                SymbolInfo info;
                info.charOffset = (int)(p - pStart);
                info.sizeInTChars = 1;
                symbols.push_back(info);
            }
            wchar_t ch = *p;
            ++charCount;
            if (IsLeadByte(ch))
            {
                SymbolInfo& info = symbols.back();
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

    // CutVisibleString
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

    // CWin32SymbolsAnalyzer_UTF16
    int CWin32SymbolsAnalyzer_UTF16::CutVisibleString(String::string_type& str,
        int visibleSymCount)
    {
        return CutStringUTF16(str, visibleSymCount);
    }

    int CWin32SymbolsAnalyzer_UTF16::CalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        const String::char_type exceptSym_in)
    {
        return CalculateSymbolsCountUTF16(pStart, sizeInWchars, exceptSym_in);
    }

    int CWin32SymbolsAnalyzer_UTF16::CalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        std::vector<SymbolInfo>& symbols)
    {
        symbols.clear();
        return CalculateSymbolsCountUTF16(pStart, sizeInWchars, symbols);
    }

    // UCS2
    int CWin32SymbolsAnalyzer_UCS2::CutVisibleString(String::string_type& str,
        int visibleSymCount)
    {
        if (str.size() > visibleSymCount)
        {
            str.resize(visibleSymCount);
            return visibleSymCount;
        }
        return (int)str.size();
    }

    int CWin32SymbolsAnalyzer_UCS2::CalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        const String::char_type exceptSym_in)
    {
        return CalculateSymbolsCountUCS2(pStart, sizeInWchars, exceptSym_in);
    }

    int CWin32SymbolsAnalyzer_UCS2::CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            std::vector<SymbolInfo>& symbols)
    {
        symbols.clear();
        symbols.resize(sizeInWchars, SymbolInfo());
        int pos = 0;
        for (auto& sym : symbols)
        {
            sym.charOffset = pos;
            sym.sizeInTChars = 1;
            ++pos;
        }
        return (int)sizeInWchars;
    }
}
