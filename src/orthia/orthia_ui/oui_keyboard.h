#pragma once

namespace oui
{
    struct KeyState
    {
        const static int AnyAlt     = 0x0001;
        const static int AnyCtrl    = 0x0002;
        const static int AnyShift   = 0x0004;
        const static int LeftAlt    = 0x0010;
        const static int LeftCtrl   = 0x0020;
        const static int LeftShift  = 0x0040;
        const static int RightAlt   = 0x0100;
        const static int RightCtrl  = 0x0200;
        const static int RightShift = 0x0400;

        int state = 0;
    };
    enum class VirtualKey
    {
        None,
        Escape,
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
        Backspace,
        Insert,
        Delete,
        Home,
        End,
        PageUp,
        PageDown,
        Break,
        CtrlC
    };
}