#pragma once
#include "oui_string.h"
#include "oui_keyboard.h"

namespace oui
{
    
    enum class MouseButton 
    {
        None = 0,
        Move = 1,
        Left = 2,
        Middle = 3,
        Right = 4,
        WheelUp = 5,
        WheelDown = 6
    };

    enum class MouseState
    {
        None = 0,
        Pressed = 1,
        DoubleClick = 2,
        Released = 3,
    };

    struct MouseEvent
    {
        bool valid = false;
        Point point;
        MouseButton button = MouseButton::None;
        MouseState state = MouseState::None;
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
        VirtualKey virtualKey = VirtualKey::None;
    };
    struct FocusEvent
    {
        bool valid = false;
        bool focusSet = false;
    };
    struct InputEvent
    {
        // common fields
        KeyState keyState;

        // keyboard events
        KeyboardEvent keyEvent;

        // mouse
        MouseEvent mouseEvent;
        
        // console 
        ResizeEvent resizeEvent;

        // focus
        FocusEvent focusEvent;
    };

    struct Hotkey
    {
        KeyState keyState;
        VirtualKey hotkey;

        Hotkey()
            :
            hotkey(VirtualKey::None)
        {
        }
        Hotkey(const KeyState& keyState_in,
               const VirtualKey& hotkey_in)
            :
                keyState(keyState_in),
                hotkey(hotkey_in)
        {
        }
        Hotkey(const VirtualKey& hotkey_in)
            :
            hotkey(hotkey_in)
        {
        }
    };

    inline bool operator == (const Hotkey& k1, const Hotkey& k2)
    {
        return k1.hotkey == k2.hotkey && k1.keyState.state == k2.keyState.state;
    }

    struct HotkeyHash
    {
        std::size_t operator()(const Hotkey& k) const
        {
            std::size_t seed = 0;
            hash_combine(seed, k.keyState.state);
            hash_combine(seed, k.hotkey);
            return seed;
        }
    };
}

#if defined(_WIN32)
#include "oui_input_win32.h"
#endif