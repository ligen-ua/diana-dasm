#include "oui_menu.h"

namespace oui
{
    static const auto g_HotKeySymbol = String::char_type('&');
    // CMenuButtonWindow
    CMenuButtonWindow::CMenuButtonWindow(const String& caption,
        std::function<void()> handler,
        std::shared_ptr<MenuColorProfile> menuColorProfile)
        :
            m_caption(caption),
            m_handler(handler),
            m_menuColorProfile(menuColorProfile)
    {
    }

    void CMenuButtonWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        auto parent = GetParent();
        if (!parent)
        {
            return;
        }
        auto parentMenu = static_cast<CMenuWindow*>(parent.get());
        bool menuFocused = parentMenu->IsActive();
        MenuButtonProfile* profile = &m_menuColorProfile->normal;
        if (menuFocused)
        {
            auto selectedButton = parentMenu->GetSelectedButton();
            if (selectedButton.get() == this)
            {
                // oh, me is selected, this is nice 
                profile = &m_menuColorProfile->selected;
            }
        }
        const auto size = GetSize();
        parameters.console.PaintRect(rect, profile->buttonBackground, false);

        auto textPos = rect.position;
        textPos.x += m_spaceAroundName;
        parameters.console.PaintText(textPos, 
            profile->buttonText, 
            profile->buttonBackground,
            m_caption,
            g_HotKeySymbol,
            profile->buttonHotkeyText,
            profile->buttonBackground);
    }
    void CMenuButtonWindow::Dock()
    {
        int symbols = CalculateSymbolsCount(m_caption.native, g_HotKeySymbol);
        Size size = { m_spaceAroundName*2 + symbols, 1 };
        this->Resize(size);
    }
   
    // CMenuWindow
    CMenuWindow::CMenuWindow()
    {
        m_menuColorProfile = std::make_shared<MenuColorProfile>();
        InitDefaultColorProfile(*m_menuColorProfile);
        Color backgroundColor;
        Color buttonText;
        Color buttonBackground;
        Color buttonHighlightedText;
        Color buttonHighlightedBackground;
    }
    void CMenuWindow::AddButton(const String& caption,
        std::function<void()> handler)
    {
        m_buttons.push_back(std::make_shared<CMenuButtonWindow>(caption, handler, m_menuColorProfile));
    }
    void CMenuWindow::ConstuctChilds()
    {
        for (auto& button : m_buttons)
        {
            AddChild(button);
        }
    }
    void CMenuWindow::Dock()
    {
        auto parent = GetParent();
        if (!parent)
        {
            return;
        }
        const auto parentSize = parent->GetSize();
        if (parentSize.height <= 0)
        {
            return;
        }
        auto size = parentSize;
        size.height = 1;
        this->Resize(size);

        // dock buttons
        int xpos = m_initialSpace;
        for (auto& button : m_buttons)
        {
            button->MoveTo({ xpos, 0 });
            button->Dock();

            const auto buttonSize = button->GetSize();
            xpos += buttonSize.width;
            xpos += m_spaceBetweenButtons;
        }
    }
    std::shared_ptr<MenuColorProfile> CMenuWindow::GetColorProfile()
    {
        return m_menuColorProfile;
    }

    std::shared_ptr<CMenuButtonWindow>  CMenuWindow::GetSelectedButton()
    {
        if (m_selectedButtonIndex < 0 || m_selectedButtonIndex >= m_buttons.size())
        {
            return 0;
        }
        return m_buttons[m_selectedButtonIndex];
    }
    void CMenuWindow::SetSelectedButtonIndex(int index)
    {
        m_selectedButtonIndex = index;
    }
    int CMenuWindow::GetSelectedButtonIndex() const
    {
        return m_selectedButtonIndex;
    }

    bool CMenuWindow::ProcessEvent(oui::InputEvent& evt) 
    {
        auto pool = GetPool();
        if (!pool)
        {
            return false;
        }
        if (evt.keyEvent.valid)
        {
            switch (evt.keyEvent.virtualKey)
            {
            case oui::VirtualKey::Escape:
                pool->SetFocus(0);
                Invalidate();
                break;
            }
        }
        return true;
    }
}