#pragma once

namespace oui
{
    class CConsoleInputReader:Noncopyable
    {
        std::vector<char> m_buffer;

        InputEvent m_notCompleted;

        std::atomic<bool> m_gotCtrlC = false;

        bool EmulateCtrlc(std::vector<InputEvent>& input);

    public:
        CConsoleInputReader();
        ~CConsoleInputReader();
        bool Read(std::vector<InputEvent>& input);
    };
}

