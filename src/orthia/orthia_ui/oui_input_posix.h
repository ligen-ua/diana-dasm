#ifdef OUI_SYS_POSIX

#pragma once

#include "oui_base_posix.h"

namespace oui
{
    class CConsoleInputReader:Noncopyable
    {

    public:
        CConsoleInputReader();
        ~CConsoleInputReader();
        void Interrupt();
        bool Read(std::vector<InputEvent>& input);
    };
}

#endif
