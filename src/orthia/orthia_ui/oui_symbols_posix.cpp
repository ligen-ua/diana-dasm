#include "oui_string.h"
#include "oui_symbols_posix.h"

namespace oui
{

// Returns the byte length of a UTF-8 codepoint given its leading byte.
// Invalid leading bytes are treated as single bytes to keep the walker moving.
static int Utf8CharLen(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

VisibleStringInfo CPosixSymbolsAnalyzer::CutVisibleString(String::string_type& str, int visibleSymCount)
{
    int charCount = 0;
    size_t i = 0;
    while (i < str.size())
    {
        if (charCount >= visibleSymCount)
        {
            str.resize(i);
            break;
        }
        int len = Utf8CharLen((unsigned char)str[i]);
        i += len;
        ++charCount;
    }
    return VisibleStringInfo(charCount, charCount);
}

int CPosixSymbolsAnalyzer::CalculateSymbolsCount(const String::char_type* pStart,
    size_t size,
    const String::char_type exceptSym_in)
{
    // exceptSym_in is the hotkey marker (ASCII). Each time it is seen, skip it
    // once and reset so the next occurrence is skipped again — matches Win32 behaviour.
    char exceptSym = exceptSym_in;
    int charCount = 0;
    size_t i = 0;
    while (i < size)
    {
        unsigned char c = (unsigned char)pStart[i];
        int len = Utf8CharLen(c);

        // Only ASCII single-byte chars can match the hotkey marker
        if (len == 1 && pStart[i] == exceptSym)
        {
            exceptSym = 0;  // skip only once
            i += len;
            continue;
        }
        ++charCount;
        exceptSym = exceptSym_in;  // reset for next token
        i += len;
    }
    return charCount;
}

int CPosixSymbolsAnalyzer::CalculateSymbolsCount(const String::char_type* pStart,
    size_t size,
    std::vector<SymbolInfo>& symbols)
{
    symbols.clear();
    int charCount = 0;
    size_t i = 0;
    int visibleOffset = 0;
    while (i < size)
    {
        unsigned char c = (unsigned char)pStart[i];
        int len = Utf8CharLen(c);

        SymbolInfo info;
        info.charOffset    = (int)i;
        info.sizeInTChars  = (int16_t)len;
        info.visibleOffset = visibleOffset;
        info.visibleSize   = 1;  // assume 1 column per codepoint (safe for non-CJK)

        symbols.push_back(info);
        ++charCount;
        ++visibleOffset;
        i += len;
    }
    return charCount;
}

}
