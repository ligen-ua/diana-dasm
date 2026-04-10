#ifdef OUI_SYS_POSIX

#pragma once

#include "oui_base_posix.h"
#include <memory>

namespace oui
{
    class CConsoleInputReader:Noncopyable
    {
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    public:
        CConsoleInputReader();
        ~CConsoleInputReader();
        void Interrupt();
        bool Read(std::vector<InputEvent>& input);
    };
}

#endif
