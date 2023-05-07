#include "oui_symbols_newterm_win32.h"

namespace oui
{
    static
    bool isAsianSymbol(wchar_t ch)
    {
        if ((ch & 0x00FF) == ch)
            return false;

        // Check if the character is in the range of CJK Unified Ideographs
        if (ch >= 0x4E00 && ch <= 0x9FFF) {
            return true;
        }

        // Check if the character is in the range of Hiragana
        if (ch >= 0x3040 && ch <= 0x309F) {
            return true;
        }

        // Check if the character is in the range of Katakana
        if (ch >= 0x30A0 && ch <= 0x30FF) {
            return true;
        }

        // Check if the character is in the range of Hangul Jamo
        if (ch >= 0x1100 && ch <= 0x11FF) {
            return true;
        }

        // Check if the character is in the range of Hangul Syllables
        if (ch >= 0xAC00 && ch <= 0xD7AF) {
            return true;
        }

        // Check if the character is in the range of Hangul Compatibility Jamo
        if (ch >= 0x3130 && ch <= 0x318F) {
            return true;
        }

        // Check if the character is in the range of CJK Compatibility Ideographs
        if (ch >= 0xF900 && ch <= 0xFAFF) {
            return true;
        }

        // Check if the character is in the range of CJK Compatibility Forms
        if (ch >= 0xFE30 && ch <= 0xFE4F) {
            return true;
        }

        // Check if the character is in the range of Bopomofo
        if (ch >= 0x3100 && ch <= 0x312F) {
            return true;
        }

        // Check if the character is in the range of Kanbun
        if (ch >= 0x3190 && ch <= 0x319F) {
            return true;
        }

        // Check if the character is in the range of Bopomofo Extended
        if (ch >= 0x31A0 && ch <= 0x31BF) {
            return true;
        }

        // Check if the character is in the range of Katakana Phonetic Extensions
        if (ch >= 0x31F0 && ch <= 0x31FF) {
            return true;
        }

        // Check if the character is in the range of Enclosed CJK Letters and Months
        if (ch >= 0x3200 && ch <= 0x32FF) {
            return true;
        }

        // Check if the character is in the range of CJK Radicals Supplement
        if (ch >= 0x2E80 && ch <= 0x2EFF) {
            return true;
        }

        // Check if the character is in the range of Kangxi Radicals
        if (ch >= 0x2F00 && ch <= 0x2FDF) {
            return true;
        }

        // Check if the character is in the range of Ideographic Description Characters
        if (ch >= 0x2FF0 && ch <= 0x2FFF) {
            return true;
        }
        return false;
    }


    static bool CheckHasWideSymbols(const String::char_type* pStart,
        size_t sizeInWchars)
    {
        for (size_t i = 0; i < sizeInWchars; ++i)
        {
            wchar_t ch = pStart[i];
            if (IsLeadByte(ch))
            {
                return true;
            }
            if (isAsianSymbol(ch))
            {
                return true;
            }
        }
        return false;
    }

    int TermCalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        std::vector<SymbolInfo>& symbols,
        int visibleLimit,
        const String::char_type exceptSym_in,
        std::function<int (const wchar_t *, const wchar_t *)> calcSize)
    {
        symbols.clear();

        wchar_t exceptSym = exceptSym_in;
        int charCount = 0;
        const wchar_t* pEnd = pStart + sizeInWchars;
        bool prevWasLead = false;
        int visibleCount = 0;
        for (const wchar_t* p = pStart; p < pEnd; ++p)
        {
            wchar_t ch = *p;
            if (ch == exceptSym)
            {
                exceptSym = 0;
                continue;
            }
            if (prevWasLead)
            {
                SymbolInfo& info = symbols.back();
                ++info.sizeInTChars;
                ++charCount;
                prevWasLead = false;
                continue;
            }
            {
                SymbolInfo info;
                info.charOffset = (int)(p - pStart);
                info.sizeInTChars = 1;

                info.visibleOffset = visibleCount;
                info.visibleSize = calcSize(p, pEnd);

                visibleCount += info.visibleSize;

                if (visibleLimit != -1 && visibleCount > visibleLimit)
                {
                    break;
                }
                symbols.push_back(info);
            }
            ++charCount;
            prevWasLead = IsLeadByte(ch);
        }
        return charCount;
    }
    CWin32SymbolsAnalyzer_NewTerminal::CWin32SymbolsAnalyzer_NewTerminal(HWND hWindow)
        :
            m_hWindow(hWindow)
    {
        m_calcSize = [=](auto begin, auto end) { return AnalyzeSymbolSize(begin, end);  };
        m_windowDC = GetDC(m_hWindow);
    }
    CWin32SymbolsAnalyzer_NewTerminal::~CWin32SymbolsAnalyzer_NewTerminal()
    {
        ReleaseDC(m_hWindow, m_windowDC);
    }
    int CWin32SymbolsAnalyzer_NewTerminal::AnalyzeSymbolSize(const String::char_type* pStart, 
        const String::char_type* pEnd)
    {
        SIZE size = { 0,0 };

        int textSize = 1;
        if (IsLeadByte(*pStart) && (pEnd - pStart) > 1)
        {
            textSize = 2;
        }
        GetTextExtentPoint32W(m_windowDC, pStart, textSize, &size);
        if (size.cx > size.cy)
        {
            return 2;
        }
        return 1;
    }
    VisibleStringInfo CWin32SymbolsAnalyzer_NewTerminal::CutVisibleString(String::string_type& str,
        int visibleSymCount)
    {
        if (!CheckHasWideSymbols(str.c_str(), str.size()))
        {
            return m_utf16.CutVisibleString(str, visibleSymCount);
        }

        TermCalculateSymbolsCount(str.c_str(),
            str.size(),
            m_symbolsBuf,
            visibleSymCount,
            0,
            m_calcSize);

        if (m_symbolsBuf.empty())
        {
            str.clear();
            return VisibleStringInfo(0, 0);
        }
        auto last = m_symbolsBuf.back();
        int charsCount = last.charOffset + last.sizeInTChars;
        str.resize(charsCount);
        int visibleCount = last.visibleOffset + last.visibleSize;
        return VisibleStringInfo(visibleCount, (int)m_symbolsBuf.size());
    }

    int CWin32SymbolsAnalyzer_NewTerminal::CalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        const String::char_type exceptSym_in)
    {        
        return m_utf16.CalculateSymbolsCount(pStart, sizeInWchars, exceptSym_in);
    }

    int CWin32SymbolsAnalyzer_NewTerminal::CalculateSymbolsCount(const String::char_type* pStart,
        size_t sizeInWchars,
        std::vector<SymbolInfo>& symbols)
    {
        if (!CheckHasWideSymbols(pStart, sizeInWchars))
        {
            return m_utf16.CalculateSymbolsCount(pStart, sizeInWchars, symbols);
        }
        return TermCalculateSymbolsCount(pStart,
            sizeInWchars,
            symbols,
            -1,
            0,
            m_calcSize);
    }


}
