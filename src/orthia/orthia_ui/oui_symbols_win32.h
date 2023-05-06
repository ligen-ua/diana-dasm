#pragma once

#include "oui_string.h"

namespace oui
{
    class CWin32SymbolsAnalyzer_UTF16:public ISymbolsAnalyzer
    {
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


    class CWin32SymbolsAnalyzer_UCS2:public ISymbolsAnalyzer
    {
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
