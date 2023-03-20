#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"
#include "oui_modal.h"

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

        void Dock();

        std::shared_ptr<const std::vector<PopupItem>> GetPopupItems();
    };

    class CMenuWindow;
    class CMenuPopup:public WithBorder<CModalWindow>
    {
        using Parent_type = WithBorder<CModalWindow>;

        std::weak_ptr<CMenuWindow> m_menuWindow;
        std::shared_ptr<MenuColorProfile> m_menuColorProfile;

        int m_selectedPosition = 0;
        void ShiftIndex(int difference);

        void FireEvent();

    public:
        CMenuPopup(std::shared_ptr<CMenuWindow> menuWindow);
        bool ProcessEvent(oui::InputEvent& evt) override;
        void Dock();
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
    };

    class CMenuWindow:public oui::SimpleBrush<CWindow>
    {
        std::vector<std::shared_ptr<CMenuButtonWindow>> m_buttons;
        // ui
        int m_initialSpace = 2;
        int m_spaceBetweenButtons = 0;
        
        int m_selectedButtonIndex = 0;
        std::shared_ptr<MenuColorProfile> m_menuColorProfile;
    
        std::weak_ptr<CWindow> m_prevFocus;

        std::shared_ptr<CMenuPopup> m_currentPopup;

    public:
        CMenuWindow();
        void AddButton(const String& caption,
            std::function<void()> handler);
        void AddButton(const String& caption,
            std::vector<PopupItem>&& items);
        void ConstuctChilds() override;
        void Dock();
        std::shared_ptr<MenuColorProfile> GetColorProfile();

        std::shared_ptr<CMenuButtonWindow>  GetSelectedButton();
        
        void SetSelectedButtonIndex(int index);
        int GetSelectedButtonIndex() const;
        void ShiftSelectedButtonIndex(int difference);

        bool ProcessEvent(oui::InputEvent& evt) override;

        void SetPrevFocus(std::shared_ptr<CWindow> prevFocus);
        void Activate() override;
        void Deactivate() override;

        void OpenPopup();
    };

}