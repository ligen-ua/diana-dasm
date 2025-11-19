#pragma once

#include "oui_input.h"

namespace oui
{
    class CHotkeyStorage
    {
        using HotkeyMap = std::unordered_map<Hotkey, std::function<void()>, HotkeyHash>;
        HotkeyMap m_hotkeys;

    public:
        void Clear();
        void Register(const Hotkey& key, std::function<void()> handler);
        bool ProcessEvent(InputEvent& evt);
        std::function<void()> QueryHandler(const Hotkey& hotkey);
    };
}