#pragma once

#include "oui_base.h"

namespace oui
{
    struct String
    {
#ifdef OUI_SYS_WINDOWS
        std::wstring native;
#define OUI_TCSTR(X) L##X
#define OUI_STR(X) std::wstring(OUI_TCSTR(X))
#define OUI_TO_STR(X) std::to_wstring(X)
#define OUI_SCANF swscanf
#define OUI_STRNCMP(X1, X2, X3)  wcsncmp(X1, X2, X3)

#else
        std::string native;
#define OUI_TCSTR(X) X
#define OUI_STR(X) std::string(X)
#define OUI_TO_STR(X) std::to_string(X)
#define OUI_SCANF sscanf
#define OUI_STRNCMP(X1, X2, X3)  strncmp(X1, X2, X3)

#endif
        typedef typename decltype(native)::value_type char_type;
        typedef decltype(native) string_type;
        const static char_type symSpace = (char_type)' ';

        String()
        {
        }
        String(const string_type& str)
            :
            native(str)
        {
        }
        String(const char_type * p)
            :
            native(p)
        {
        }
    };



    // symbols
    struct SymbolInfo
    {
        int charOffset = 0;
        int visibleOffset = 0;
        int16_t sizeInTChars = 0;
        int16_t visibleSize = 0;
    };

    struct ISymbolsAnalyzer
    {
        virtual ~ISymbolsAnalyzer() {}
        
        virtual int CutVisibleString(String::string_type& str, 
            int visibleSymCount) = 0;
        
        virtual int CalculateSymbolsCount(const String::char_type* pStart, 
            size_t sizeInWchars, 
            const String::char_type exceptSym_in) = 0;
        
        virtual int CalculateSymbolsCount(const String::char_type* pStart,
            size_t sizeInWchars,
            std::vector<SymbolInfo>& symbols) = 0;

        template<class Type, class CharType>
        int CalculateSymbolsCount(const Type& str, const CharType exceptSym_in)
        {
            return this->CalculateSymbolsCount(str.c_str(), str.size(), exceptSym_in);
        }
    };
}