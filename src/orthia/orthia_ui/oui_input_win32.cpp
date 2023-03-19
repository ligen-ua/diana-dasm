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

        if ((state.state & state.AnyCtrl) && !(state.state& state.AnyAlt))
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
    static bool TranslateKeyEvent(INPUT_RECORD& raw, InputEvent& evt)
    {
        if (!raw.Event.KeyEvent.bKeyDown)
        {
            return false;
        }

        //printf("wVirtualKeyCode: VK = %x, %x, %x\n", (int)raw.Event.KeyEvent.wVirtualKeyCode,
        //    (int)raw.Event.KeyEvent.wVirtualKeyCode, (int)raw.Event.KeyEvent.wVirtualScanCode);
        evt.keyState = TranslateKeyState(raw.Event.KeyEvent.dwControlKeyState);
        evt.keyEvent.virtualKey = TranslateVirtualKey(raw.Event.KeyEvent.wVirtualKeyCode, evt.keyState);
        if (evt.keyEvent.virtualKey == VirtualKey::None)
        {
            if (raw.Event.KeyEvent.uChar.UnicodeChar)
            {
                evt.keyEvent.rawText.native.append(&raw.Event.KeyEvent.uChar.UnicodeChar, 1);
                return true;
            }
            return false;
        }
        return true;
    }
    static bool TranslateEvent(INPUT_RECORD& raw, InputEvent& evt)
    {
        if (raw.EventType & KEY_EVENT)
        {
            evt.keyEvent.valid = TranslateKeyEvent(raw, evt);
        }
        if (raw.EventType & MOUSE_EVENT)
        {
            evt.keyState = TranslateKeyState(raw.Event.MouseEvent.dwControlKeyState);
        }
        if (raw.EventType & WINDOW_BUFFER_SIZE_EVENT)
        {
            evt.resizeEvent.valid = true;
            evt.resizeEvent.newWidth = raw.Event.WindowBufferSizeEvent.dwSize.X;
            evt.resizeEvent.newHeight = raw.Event.WindowBufferSizeEvent.dwSize.Y;
        }
        return evt.keyEvent.valid || evt.resizeEvent.valid || evt.mouse.valid;
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
    bool CConsoleInputReader::Read(std::vector<InputEvent>& input)
    {
        for (;;)
        {
            input.clear();

            const int msToWait = 500;
            const int pageSize = 256;
            m_buffer.resize(sizeof(INPUT_RECORD) * pageSize);

            auto conHandle = GetStdHandle(STD_INPUT_HANDLE);
            for (;;)
            {
                if (WAIT_OBJECT_0 == WaitForSingleObject(conHandle, msToWait))
                {
                    // there is data
                    break;
                }
                if (m_gotCtrlC)
                {
                    break;
                }
            }

            EmulateCtrlc(input);

            PINPUT_RECORD bufferStart = (PINPUT_RECORD)m_buffer.data();
            DWORD eventsCount = 0;
            BOOL res = ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE),
                bufferStart,
                pageSize,
                &eventsCount);
            if (!res)
            {
                return false;
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
                    event.keyEvent.virtualKey == VirtualKey::None &&
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
            if (!input.empty())
            {
                return true;
            }
        }
    }


}
