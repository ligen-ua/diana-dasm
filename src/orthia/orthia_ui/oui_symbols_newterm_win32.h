#pragma once

#include "oui_symbols_win32.h"

namespace oui
{

    class CWin32SymbolsAnalyzer_NewTerminal:public ISymbolsAnalyzer
    {
        HWND m_hWindow;
        HDC m_windowDC;
        CWin32SymbolsAnalyzer_UCS2 m_utf16;

        std::function<int(const wchar_t*, const wchar_t*)> m_calcSize;

        std::vector<SymbolInfo> m_symbolsBuf;

        int AnalyzeSymbolSize(const String::char_type* pStart, const String::char_type* pEnd);
    public:
        CWin32SymbolsAnalyzer_NewTerminal(HWND hWindow);
        ~CWin32SymbolsAnalyzer_NewTerminal();
        VisibleStringInfo CutVisibleString(String::string_type& str,
            int visibleSymCount) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            const String::char_type exceptSym_in) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            std::vector<SymbolInfo>& symbols) override;
    };

}
