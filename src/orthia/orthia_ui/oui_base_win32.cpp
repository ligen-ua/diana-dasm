#include "oui_base.h"
#include "oui_base_win32.h"

namespace oui
{
    void LogOutput(LogFlags flags, const std::string& text)
    {
        auto result = std::string("[") + ToStringA(flags) + "] " + text + "\n";
        OutputDebugStringA(result.c_str());
    }
    void LogOutput(LogFlags flags, const std::wstring& text)
    {
        auto result = std::wstring(L"[") + ToStringW(flags) + L"] " + text + L"\n";
        OutputDebugStringW(result.c_str());
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