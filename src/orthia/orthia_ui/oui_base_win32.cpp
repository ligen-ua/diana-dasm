#include "oui_base.h"
#include "oui_base_win32.h"

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
        return (int)sizeInWchars;
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

    void FilterUnreadableSymbols(std::wstring& text)
    {
        text.erase(std::remove_if(text.begin(), text.end(), [](wchar_t ch) {
            if ((unsigned int)ch < (unsigned int)' ')
            {
                return true;
            }
        return false;

        }), text.end());
    }

    std::wstring Uppercase_Silent(const std::wstring& str)
    {
        if (str.empty())
            return std::wstring();

        std::vector<wchar_t> temp(str.c_str(), str.c_str() + str.size());
        DWORD dwSize = (DWORD)(str.size());
        if (CharUpperBuffW(&temp.front(), dwSize) != dwSize)
        {
            return str;
        }
        return std::wstring(&temp.front(), &temp.front() + dwSize);
    }
    bool StartsWith(const std::wstring& text, const std::wstring& phrase)
    {
        if (phrase.size() > text.size())
        {
            return false;
        }
        if (phrase.size() == text.size())
        {
            auto textUp = Uppercase_Silent(text);
            auto phraseUp = Uppercase_Silent(phrase);
            return textUp == phraseUp;
        }
        auto textUp = Uppercase_Silent(std::wstring(text.begin(), text.begin() + phrase.size()));
        auto phraseUp = Uppercase_Silent(std::wstring(phrase.begin(), phrase.begin() + phrase.size()));
        return textUp == phraseUp;

    }

}