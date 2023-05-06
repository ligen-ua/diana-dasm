#pragma once

#include "oui_symbols_win32.h"

namespace oui
{

    class CWin32SymbolsAnalyzer_NewTerminal:public ISymbolsAnalyzer
    {
        CWin32SymbolsAnalyzer_UTF16 m_utf16;
    public:
        int CutVisibleString(String::string_type& str,
            int visibleSymCount) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            const String::char_type exceptSym_in) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            std::vector<SymbolInfo>& symbols) override;
    };

}
