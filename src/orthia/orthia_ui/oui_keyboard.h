#pragma once

namespace oui
{
    struct KeyState
    {
        const int AnyAlt     = 0x0001;
        const int AnyCtrl    = 0x0002;
        const int AnyShift   = 0x0004;
        const int LeftAlt    = 0x0010;
        const int LeftCtrl   = 0x0020;
        const int LeftShift  = 0x0040;
        const int RightAlt   = 0x0100;
        const int RightCtrl  = 0x0200;
        const int RightShift = 0x0400;

        int state = 0;
    };
    enum class VirtualKey
    {
        None,
        kF1,
        kF2,
        kF3,
        kF4,
        kF5,
        kF6,
        kF7,
        kF8,
        kF9,
        kF10,
        kF11,
        kF12,
        Left,
        Right,
        Up,
        Down,
        Tab,
        Enter,
        Backspace
    };
}