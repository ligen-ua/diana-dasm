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
        KeyState()
        {
        }
        KeyState(int state_in)
            :
                state(state_in)
        {
        }
        bool HasModifiers() const
        {
            return state & (AnyAlt | AnyCtrl | AnyShift);
        }
    };
    enum class VirtualKey
    {
        None = 0,
        kA = 1,
        kB = 2,
        kС = 3,
        kD = 4,
        kE = 5,
        kF = 6,
        kG = 7,
        kH = 8,
        kI = 9,
        kJ = 10,
        kK = 11,
        kL = 12,
        kM = 13,
        kN = 14,
        kO = 15,
        kP = 16,
        kQ = 17,
        kR = 18,
        kS = 19,
        kT = 20,
        kU = 21,
        kV = 22,
        kW = 23,
        kX = 24,
        kY = 25,
        kZ = 26,
        k0 = 27,
        k1 = 28,
        k2 = 29,
        k3 = 30,
        k4 = 31,
        k5 = 32,
        k6 = 33,
        k7 = 34,
        k8 = 35,
        k9 = 36,
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