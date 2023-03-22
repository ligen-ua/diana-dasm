#pragma once
#include <windows.h>
namespace oui
{
    class CConsoleInputReader:Noncopyable
    {
        std::vector<char> m_buffer;

        InputEvent m_notCompleted;

        std::atomic<bool> m_gotCtrlC = false;
        bool m_altHotkeyHappened = false;

        bool EmulateCtrlc(std::vector<InputEvent>& input);
        bool TranslateKeyEvent(INPUT_RECORD& raw, InputEvent& evt);
        bool TranslateEvent(INPUT_RECORD& raw, InputEvent& evt);

    public:
        CConsoleInputReader();
        ~CConsoleInputReader();
        bool Read(std::vector<InputEvent>& input);
    };
}

