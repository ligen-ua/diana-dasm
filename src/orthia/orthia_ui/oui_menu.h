#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_modal.h"
#include "oui_hotkey.h"

namespace oui
{

    struct PopupItem
    {
        String text;
        std::function<void()> handler;
        Hotkey hotkey;
    };

    class CMenuButtonWindow:public CWindow
    {
        String m_caption;
        std::function<void()> m_handler;
        int m_spaceAroundName = 2;

        std::vector<PopupItem> m_items;
    public:
        CMenuButtonWindow(const String& caption,
            std::function<void()> handler,
            std::vector<PopupItem>&& items);

        std::function<void()> & GetHandler();

        void DoPaint(const Rect& rect, 
            DrawParameters& parameters) override;

        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        void Dock();
        std::shared_ptr<const std::vector<PopupItem>> GetPopupItems();
    };

    class CMenuWindow;
    class CMenuPopup:public WithBorder<CBaseModalWindow>
    {
        using Parent_type = WithBorder<CBaseModalWindow>;

        std::weak_ptr<CMenuWindow> m_menuWindow;
        std::shared_ptr<MenuColorProfile> m_menuColorProfile;
        CHotkeyStorage m_hotkeys;

        int m_selectedPosition = 0;
        bool m_destroyed = false;
        void ShiftIndex(int difference);
        void FireEvent();
        void UpdateHotkeys(std::shared_ptr<CWindow> menu, const std::vector<PopupItem>& items);
    public:
        CMenuPopup(std::shared_ptr<CMenuWindow> menuWindow);
        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;
        void Dock();
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
        void Destroy() override;
        void Detach();
        bool IsPopup() const override { return true; }
        void OnFocusLost() override;
        std::shared_ptr<CWindow> GetPopupPrevFocusTarget() override;

    };

    class CMenuWindow:public oui::SimpleBrush<CWindow>
    {
        using Parent_type = oui::SimpleBrush<CWindow>;

        std::vector<std::shared_ptr<CMenuButtonWindow>> m_buttons;
        // ui
        int m_initialSpace = 2;
        int m_spaceBetweenButtons = 0;
        
        int m_selectedButtonIndex = 0;
        std::shared_ptr<MenuColorProfile> m_menuColorProfile;
    
        std::weak_ptr<CWindow> m_prevFocus;

        std::shared_ptr<CMenuPopup> m_currentPopup;

        bool m_setFocusOnDeactivate = true;

        int m_enterStateIndex = -1;
    public:
        CMenuWindow();
        std::shared_ptr<CMenuButtonWindow> AddButton(const String& caption,
            std::function<void()> handler);
        std::shared_ptr<CMenuButtonWindow> AddButton(const String& caption,
            std::vector<PopupItem>&& items);
        void ConstructChilds() override;
        void Dock();
        std::shared_ptr<MenuColorProfile> GetColorProfile();

        std::shared_ptr<CMenuButtonWindow> GetSelectedButton();
        
        void SetSelectedButtonIndex(int index);
        int GetSelectedButtonIndex() const;
        void ShiftSelectedButtonIndex(int difference);

        bool ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext) override;

        void SetPrevFocus(std::shared_ptr<CWindow> prevFocus);
        void Activate() override;
        void Deactivate() override;
        std::shared_ptr<CWindow> GetPopupPrevFocusTarget() override;

        void OpenPopup();

        bool PopupIsOpen() const;

        // enter state means: do not close on mouse release
        void SelectAndOpenPopup(std::shared_ptr<CMenuButtonWindow> button, bool setEnterState = true);
        void OnFocusLost() override;

        void DontSetFocusOnDeactivate();

        bool ClearEnterState(std::shared_ptr<CMenuButtonWindow> button);

    };

}