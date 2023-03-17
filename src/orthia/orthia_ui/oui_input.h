#pragma once
#include "oui_string.h"
#include "oui_keyboard.h"

namespace oui
{
    struct MouseEvent
    {
        bool valid = false;
        Point point;
    };

    struct ResizeEvent
    {
        bool valid = false;
        int newWidth = 0;
        int newHeight = 0;
    };

    struct InputEvent
    {
        // common fields
        KeyState keyState;

        // keyboard events
        String rawText;
        VirtualKey virtualKey;

        // mouse
        MouseEvent mouse;
        
        // console 
        ResizeEvent resizeEvent;
    };
    class CConsoleInputReader
    {
    public:
        CConsoleInputReader();
        bool Read(std::vector<InputEvent>& input);
    };
}
