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

    struct KeyboardEvent
    {
        bool valid = false;
        String rawText;
        VirtualKey virtualKey;
    };

    struct InputEvent
    {
        // common fields
        KeyState keyState;

        // keyboard events
        KeyboardEvent keyEvent;

        // mouse
        MouseEvent mouse;
        
        // console 
        ResizeEvent resizeEvent;
    };

    struct Hotkey
    {
        KeyState keyState;
        VirtualKey hotkey;
    };
}

#if defined(_WIN32)
#include "oui_input_win32.h"
#endif