#include "oui_hotkey.h"

namespace oui
{

    void CHotkeyStorage::Clear()
    {
        m_hotkeys.clear();
    }
    void CHotkeyStorage::Register(const Hotkey& key, std::function<void()> handler)
    {
        m_hotkeys[key] = handler;
    }
    bool CHotkeyStorage::ProcessEvent(InputEvent& evt)
    {
        if (!evt.keyEvent.valid)
        {
            return false;
        }
        if (evt.keyEvent.virtualKey == VirtualKey::None)
        {
            return false;
        }
        Hotkey hotkey(evt.keyState, evt.keyEvent.virtualKey);
        auto handler = QueryHandler(hotkey);
        if (!handler)
        {
            return false;
        }
        handler();
        return true;
    }

    static bool ClearLeftRight(const Hotkey& hotkey, Hotkey& result)
    {
        bool res = false;
        result = hotkey;
        if (hotkey.keyState.state & (KeyState::LeftAlt | KeyState::RightAlt))
        {
            result.keyState.state &= ~(KeyState::LeftAlt | KeyState::RightAlt);
            result.keyState.state |= KeyState::AnyAlt;
            res = true;
        }
        if (hotkey.keyState.state & (KeyState::LeftCtrl | KeyState::RightCtrl))
        {
            result.keyState.state &= ~(KeyState::LeftCtrl | KeyState::RightCtrl);
            result.keyState.state |= KeyState::AnyCtrl;
            res = true;
        }
        if (hotkey.keyState.state & (KeyState::LeftShift | KeyState::RightShift))
        {
            result.keyState.state &= ~(KeyState::LeftShift | KeyState::RightShift);
            result.keyState.state |= KeyState::AnyShift;
            res = true;
        }
        return res;
    }
    std::function<void()> CHotkeyStorage::QueryHandler(const Hotkey& hotkey)
    {
        auto it = m_hotkeys.find(hotkey);
        if (it != m_hotkeys.end())
        {
            return it->second;
        }

        Hotkey general;
        if (ClearLeftRight(hotkey, general))
        {
            it = m_hotkeys.find(general);
            if (it != m_hotkeys.end())
            {
                return it->second;
            }
        }
        return nullptr;
    }
}