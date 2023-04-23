#include "oui_input.h"
#include "windows.h"


namespace oui
{

    static KeyState TranslateKeyState(DWORD rawState)
    {
        KeyState res;
        // ctrl
        if (rawState & LEFT_CTRL_PRESSED)
        {
            res.state |= KeyState::LeftCtrl;
            res.state |= KeyState::AnyCtrl;
        }
        if (rawState & RIGHT_CTRL_PRESSED)
        {
            res.state |= KeyState::RightCtrl;
            res.state |= KeyState::AnyCtrl;
        }
        // alt
        if (rawState & LEFT_ALT_PRESSED)
        {
            res.state |= KeyState::LeftAlt;
            res.state |= KeyState::AnyAlt;
        }
        if (rawState & RIGHT_ALT_PRESSED)
        {
            res.state |= KeyState::RightAlt;
            res.state |= KeyState::AnyAlt;
        }
        // shift
        if (rawState & SHIFT_PRESSED)
        {
            res.state |= KeyState::LeftShift;
            res.state |= KeyState::AnyShift;
        }
        return res;
    }
    static VirtualKey TranslateVirtualKey(WORD wVirtualKeyCode, KeyState state)
    {
        if (wVirtualKeyCode >= VK_F1 && wVirtualKeyCode <= VK_F12)
        {
            return (VirtualKey)((int)VirtualKey::kF1 + (wVirtualKeyCode - VK_F1));
        }
        if (wVirtualKeyCode >= 'A' && wVirtualKeyCode <= 'Z')
        {
            return (VirtualKey)((int)VirtualKey::kA + (wVirtualKeyCode - 'A'));
        }
        if (wVirtualKeyCode >= '0' && wVirtualKeyCode <= '9')
        {
            return (VirtualKey)((int)VirtualKey::k0 + (wVirtualKeyCode - '0'));
        }
        if ((state.state & state.AnyCtrl) && !(state.state & state.AnyAlt))
        {
            if (wVirtualKeyCode == 'C')
            {
                return VirtualKey::CtrlC;
            }
        }

        switch (wVirtualKeyCode)
        {
        case VK_ESCAPE:
            return VirtualKey::Escape;

        case VK_LEFT:
            return VirtualKey::Left;

        case VK_RIGHT:
            return VirtualKey::Right;

        case VK_UP:
            return VirtualKey::Up;

        case VK_DOWN:
            return VirtualKey::Down;

        case VK_TAB:
            return VirtualKey::Tab;

        case VK_RETURN:
            return VirtualKey::Enter;

        case VK_BACK:
            return VirtualKey::Backspace;

        case VK_INSERT:
            return VirtualKey::Insert;

        case VK_DELETE:
            return VirtualKey::Delete;

        case VK_HOME:
            return VirtualKey::Home;

        case VK_END:
            return VirtualKey::End;

        case VK_PRIOR:
            return VirtualKey::PageUp;

        case VK_NEXT:
            return VirtualKey::PageDown;

        case VK_CANCEL:
            return VirtualKey::Break;
        }
        return VirtualKey::None;
    }
    bool CConsoleInputReader::TranslateKeyEvent(INPUT_RECORD& raw, InputEvent& evt)
    {
        if (!raw.Event.KeyEvent.bKeyDown)
        {
            // handle the case when user just presses ALT
            if (raw.Event.KeyEvent.wVirtualKeyCode == VK_MENU && !evt.keyState.state)
            {
                if (!m_altHotkeyHappened)
                {
                    evt.keyState.state |= evt.keyState.LeftAlt | evt.keyState.AnyAlt;
                    evt.keyEvent.virtualKey = VirtualKey::None;
                    return true;
                }
            }
            return false;
        }

        //printf("wVirtualKeyCode: VK = %x, %x, %x\n", (int)raw.Event.KeyEvent.wVirtualKeyCode,
        //    (int)raw.Event.KeyEvent.wVirtualKeyCode, (int)raw.Event.KeyEvent.wVirtualScanCode);
        evt.keyState = TranslateKeyState(raw.Event.KeyEvent.dwControlKeyState);
        evt.keyEvent.virtualKey = TranslateVirtualKey(raw.Event.KeyEvent.wVirtualKeyCode, evt.keyState);
        if (evt.keyEvent.virtualKey == VirtualKey::None)
        {
            if (raw.Event.KeyEvent.wVirtualKeyCode == VK_MENU &&
                !((KeyState::AnyCtrl | KeyState::AnyShift) & evt.keyState.state))
            {
                // we will handle this at up
                return false;
            }
            m_altHotkeyHappened = false;
        }
        else
        {
            m_altHotkeyHappened = evt.keyState.state & evt.keyState.AnyAlt;
        }
        if (raw.Event.KeyEvent.uChar.UnicodeChar)
        {
            evt.keyEvent.rawText.native.append(&raw.Event.KeyEvent.uChar.UnicodeChar, 1);
        }
        return true;
    }

    static MouseButton GetMouseButton(MOUSE_EVENT_RECORD& mouseEvent)
    {
        if (mouseEvent.dwEventFlags == MOUSE_MOVED)
        {
            return MouseButton::Move;
        }
        if (mouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED)
        {
            return MouseButton::Left;
        }
        if (mouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED)
        {
            return MouseButton::Right;
        }
        return MouseButton::Move;
    }
    static MouseState GetMouseState(MOUSE_EVENT_RECORD& mouseEvent)
    {
        if (mouseEvent.dwEventFlags & DOUBLE_CLICK)
        {
            return MouseState::DoubleClick;
        }
        if (mouseEvent.dwButtonState == 0)
        {
            if (mouseEvent.dwEventFlags == 0)
                return MouseState::Released;
            else
                return MouseState::None;
        }
        return MouseState::Pressed;
    }
    bool CConsoleInputReader::TranslateEvent(INPUT_RECORD& raw, InputEvent& evt)
    {
        switch (raw.EventType)
        {
        case FOCUS_EVENT:
            evt.focusEvent.valid = true;
            evt.focusEvent.focusSet = raw.Event.FocusEvent.bSetFocus;
            break;

        case KEY_EVENT:
            evt.keyEvent.valid = TranslateKeyEvent(raw, evt);
            break;

        case MOUSE_EVENT:
            {
                evt.keyState = TranslateKeyState(raw.Event.MouseEvent.dwControlKeyState);
                evt.mouseEvent.button = GetMouseButton(raw.Event.MouseEvent);
                evt.mouseEvent.state = GetMouseState(raw.Event.MouseEvent);

                // console api returns Move + Released this is frustrating
                // I would rather like to have mouse button there
                if (evt.mouseEvent.button == MouseButton::Move &&
                    evt.mouseEvent.state == MouseState::Released)
                {
                    if (m_lastMouseButton != MouseButton::None)
                    {
                        evt.mouseEvent.button = m_lastMouseButton;
                        m_lastMouseButton = MouseButton::None;
                    }
                }
                if (evt.mouseEvent.button != MouseButton::None || evt.mouseEvent.state != MouseState::None)
                {
                    if (evt.mouseEvent.button != MouseButton::Move)
                    {
                        m_lastMouseButton = evt.mouseEvent.button;
                    }
                    evt.mouseEvent.valid = true;
                    evt.mouseEvent.point.x = raw.Event.MouseEvent.dwMousePosition.X;
                    evt.mouseEvent.point.y = raw.Event.MouseEvent.dwMousePosition.Y;
                }
                break;
            }
        case WINDOW_BUFFER_SIZE_EVENT:
            {
                evt.resizeEvent.valid = true;
                evt.resizeEvent.newWidth = raw.Event.WindowBufferSizeEvent.dwSize.X;
                evt.resizeEvent.newHeight = raw.Event.WindowBufferSizeEvent.dwSize.Y;
                break;
            }
        };
        return evt.keyEvent.valid || evt.resizeEvent.valid || evt.mouseEvent.valid || evt.focusEvent.valid;
    }


    // ctrlc input
    static std::function<void()> g_ctrlCandler;
    static BOOL WINAPI CtrlCHandlerRoutine(DWORD dwCtrlType)
    {
        if (g_ctrlCandler && dwCtrlType == CTRL_C_EVENT)
        {
            g_ctrlCandler();
            return TRUE;
        }
        return FALSE;
    }
    static void SetCtrlcHandler(std::function<void()> handler)
    {
        g_ctrlCandler = handler;
        SetConsoleCtrlHandler(CtrlCHandlerRoutine, TRUE);
    }
    static void ResetCtrlcHandler()
    {
        g_ctrlCandler = nullptr;
        SetConsoleCtrlHandler(CtrlCHandlerRoutine, FALSE);
    }

    CConsoleInputReader::CConsoleInputReader()
        :
           m_jobEvent(EventType::Auto)
    {
        SetCtrlcHandler([&] {

            m_gotCtrlC = true;
        });
    }
    CConsoleInputReader::~CConsoleInputReader()
    {
        ResetCtrlcHandler();
    }
    bool CConsoleInputReader::EmulateCtrlc(std::vector<InputEvent>& input)
    {
        if (!m_gotCtrlC)
        {
            return false;
        }
        m_gotCtrlC = false;

        InputEvent event;
        event.keyState.state = KeyState::LeftCtrl|KeyState::AnyCtrl;
        event.keyEvent.valid = true;
        event.keyEvent.virtualKey = VirtualKey::CtrlC;
        input.push_back(event);
        return true;
    }
    void CConsoleInputReader::Interrupt()
    {
        m_jobEvent.Set();
    }
    bool CConsoleInputReader::Read(std::vector<InputEvent>& input)
    {
        input.clear();

        const int pageSize = 256;
        m_buffer.resize(sizeof(INPUT_RECORD) * pageSize);

        auto conHandle = GetStdHandle(STD_INPUT_HANDLE);

        bool readInput = false;
        {
            HANDLE handles[] = { conHandle, m_jobEvent.GetHandle()};
            DWORD status = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
            readInput = status == WAIT_OBJECT_0;
        }

        EmulateCtrlc(input);

        PINPUT_RECORD bufferStart = (PINPUT_RECORD)m_buffer.data();
        DWORD eventsCount = 0;
        if (readInput)
        {
            BOOL res = ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE),
                bufferStart,
                pageSize,
                &eventsCount);
            if (!res)
            {
                return false;
            }
        }

        PINPUT_RECORD currentItem = bufferStart;
        for (DWORD i = 0; i < eventsCount; ++i, ++currentItem)
        {
            InputEvent event;
            if (!TranslateEvent(*currentItem, event))
            {
                continue;
            }
            if (event.keyEvent.valid &&
                event.keyEvent.rawText.native.size() == 1)
            {
                if (IsLeadByte(event.keyEvent.rawText.native[0]))
                {
                    m_notCompleted = std::move(event);
                    continue;
                }
                else
                {
                    if (m_notCompleted.keyEvent.valid)
                    {
                        m_notCompleted.keyEvent.rawText.native += event.keyEvent.rawText.native;
                        input.push_back(event);
                        m_notCompleted.keyEvent.valid = false;
                        continue;
                    }
                }
            }
            input.push_back(event);
        }
        return true;
    }


}
