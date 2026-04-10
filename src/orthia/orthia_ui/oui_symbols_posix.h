#ifdef OUI_SYS_POSIX

#pragma once

#include "oui_string.h"

namespace oui
{
    class CPosixSymbolsAnalyzer : public ISymbolsAnalyzer
    {
    public:
        VisibleStringInfo CutVisibleString(String::string_type& str,
            int visibleSymCount) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t size,
            const String::char_type exceptSym_in) override;

        int CalculateSymbolsCount(const String::char_type* pStart,
            size_t size,
            std::vector<SymbolInfo>& symbols) override;
    };
}

#endif
