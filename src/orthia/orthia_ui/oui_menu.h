#pragma once

#include "oui_window.h"
#include "oui_win_styles.h"

namespace oui
{

    class CMenuButtonWindow:public CWindow
    {
        std::shared_ptr<MenuColorProfile> m_menuColorProfile;
        String m_caption;
        std::function<void()> m_handler;
        int m_spaceAroundName = 2;
    public:
        CMenuButtonWindow(const String& caption,
            std::function<void()> handler,
            std::shared_ptr<MenuColorProfile> menuColorProfile);

        void DoPaint(const Rect& rect, DrawParameters& parameters) override;

        void Dock();
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
    public:
        CMenuWindow();
        void AddButton(const String& caption,
            std::function<void()> handler);
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
    };

}