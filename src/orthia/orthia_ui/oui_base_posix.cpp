#include "oui_base.h"
#include "oui_base_posix.h"
#include "utf8rewind.h"
#include <cctype>
#include <iostream>

namespace oui
{

void FilterUnreadableSymbols(std::string& text)
{
    // Strip ASCII control characters. Bytes >= 0x80 are UTF-8 multi-byte
    // sequence parts and must be left untouched.
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char c) {
        return c < 0x20;
    }), text.end());
}

std::string Uppercase_Silent(const std::string& str)
{
    if (str.empty())
        return str;
    int32_t errors = UTF8_ERR_NONE;
    // utf8toupper may expand the string (e.g. German ß → SS), so allocate generously.
    std::vector<char> buf((str.size() + 1) * 4);
    size_t resultSize = utf8toupper(str.c_str(), str.size(),
                                    buf.data(), buf.size() - 1,
                                    UTF8_LOCALE_DEFAULT, &errors);
    if (errors != UTF8_ERR_NONE)
        return str;  // fallback: return as-is rather than corrupt
    return std::string(buf.data(), resultSize);
}

bool StartsWith(const std::string& text, const std::string& phrase)
{
    if (phrase.empty())
        return true;
    if (text.size() < phrase.size())
        return false;
    auto textUp   = Uppercase_Silent(text.substr(0, phrase.size()));
    auto phraseUp = Uppercase_Silent(phrase);
    return textUp == phraseUp;
}

void LogOutput(LogFlags flags, const std::string& text)
{
    std::cerr << "[" << ToStringA(flags) << "] " << text << "\n";
}

void LogOutput(LogFlags flags, const std::wstring& text)
{
    // wstring is unusual on POSIX; do a best-effort narrow conversion for logging.
    std::string narrow;
    narrow.reserve(text.size());
    for (wchar_t wc : text)
    {
        narrow += (wc < 0x80) ? (char)wc : '?';
    }
    LogOutput(flags, narrow);
}

}
