#include "oui_menu.h"

namespace oui
{
    static const auto g_HotKeySymbol = String::char_type('&');

    // CMenuButtonWindow
    CMenuButtonWindow::CMenuButtonWindow(const String& caption,
        std::function<void()> handler,
        std::vector<PopupItem>&& items)
        :
        m_caption(caption),
        m_handler(handler),
        m_items(std::move(items))
    {
    }
    bool CMenuButtonWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        auto parentMenu = GetParent_t<CMenuWindow>(this);
        if (!parentMenu)
        {
            return false;
        }
        auto me = Cast_t<CMenuButtonWindow>(GetPtr());
        if (!me)
        {
            return false;
        }

        // handle move only if menu is already open
        if (evt.mouseEvent.button == MouseButton::Move)
        {
            if (!parentMenu->IsActiveOrFocused())
            {
                return true;
            }
        }
        else
        {
            if (evt.mouseEvent.button != MouseButton::Left ||
                evt.mouseEvent.state != MouseState::Pressed)
            {
                return true;
            }
            else
            {
                // check if already open
                if (parentMenu->GetSelectedButton().get() == this &&
                    parentMenu->PopupIsOpen())
                {
                    parentMenu->Deactivate();
                    return true;
                }
            }
        }
        parentMenu->SelectAndOpenPopup(me);
        return true;
    }
    void CMenuButtonWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        auto parentMenu = GetParent_t<CMenuWindow>(this);
        if (!parentMenu)
        {
            return;
        }
        bool menuFocused = parentMenu->IsActiveOrFocused();
        auto menuColorProfile = parentMenu->GetColorProfile();
        MenuButtonProfile* profile = &menuColorProfile->menu.normal;
        if (menuFocused)
        {
            auto selectedButton = parentMenu->GetSelectedButton();
            if (selectedButton.get() == this)
            {
                // oh, me is selected, this is nice 
                profile = &menuColorProfile->menu.selected;
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
        Size size = { m_spaceAroundName * 2 + symbols, 1 };
        this->Resize(size);
    }

    std::function<void()>& CMenuButtonWindow::GetHandler()
    {
        return m_handler;
    }

    std::shared_ptr<const std::vector<PopupItem>> CMenuButtonWindow::GetPopupItems()
    {
        if (m_items.empty())
        {
            return nullptr;
        }
        auto me = this->GetPtr();
        if (!me)
        {
            return nullptr;
        }
        return std::shared_ptr< const std::vector<PopupItem>>(me, &m_items);
    }

    // CMenuPopup
    CMenuPopup::CMenuPopup(std::shared_ptr<CMenuWindow> menuWindow)
        :
        m_menuWindow(menuWindow)
    {
    }
    void CMenuPopup::Detach()
    {
        m_menuWindow.reset();
    }
    void CMenuPopup::Destroy()
    {
        if (m_destroyed)
        {
            return;
        }
        m_destroyed = true;
        auto menu = m_menuWindow.lock();
        Parent_type::Destroy();
        if (menu)
        {
            menu->Deactivate();
        }
    }
    bool CMenuPopup::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return false;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return false;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            return false;
        }

        auto relativePoint = GetClientMousePoint(this, rect, evt.mouseEvent.point);
        if (relativePoint.x < 0 || relativePoint.y < 0 || relativePoint.y >= popupItems->size())
        {
            return true;
        }
        int index = relativePoint.y;
        if (!(*popupItems)[index].handler)
        {
            // separator found
            return true;
        }
        switch (evt.mouseEvent.button)
        {
        case MouseButton::Move:
            m_selectedPosition = index;
            Invalidate();
            break;
        case MouseButton::Left:
            if (evt.mouseEvent.state == MouseState::Pressed)
            {
                FireEvent();
            }
            break;
        }
        return true;
    }
    void CMenuPopup::ShiftIndex(int difference)
    {
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            return;
        }

        int itemsCount = (int)popupItems->size();
        if (!itemsCount)
        {
            m_selectedPosition = 0;
            return;
        }
        for (int i = 0; i < 2; ++i)
        {
            int newIndex = m_selectedPosition + difference;
            if (newIndex >= (int)itemsCount)
            {
                newIndex = 0;
            }
            else if (newIndex < 0)
            {
                newIndex = (int)itemsCount - 1;
            }
            m_selectedPosition = newIndex;

            if ((*popupItems)[m_selectedPosition].handler)
            {
                break;
            }
        }
    }
    void CMenuPopup::FireEvent()
    {
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            return;
        }

        int itemsCount = (int)popupItems->size();
        if (!itemsCount)
        {
            return;
        }
        auto handler = ((*popupItems)[m_selectedPosition].handler);
        if (handler)
        {
            handler();
            Destroy();
        }
    }
    bool CMenuPopup::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        auto parentMenu = m_menuWindow.lock();
        if (parentMenu)
        {
            if (m_hotkeys.ProcessEvent(evt))
            {
                return true;
            }
            if (evt.keyEvent.valid)
            {
                switch (evt.keyEvent.virtualKey)
                {
                case oui::VirtualKey::Left:
                    parentMenu->ShiftSelectedButtonIndex(-1);
                    Dock();
                    return true;
                case oui::VirtualKey::Right:
                    parentMenu->ShiftSelectedButtonIndex(1);
                    Dock();
                    return true;
                case oui::VirtualKey::Down:
                    ShiftIndex(1);
                    Invalidate();
                    return true;
                case oui::VirtualKey::Up:
                    ShiftIndex(-1);
                    Invalidate();
                    return true;
                case oui::VirtualKey::Enter:
                    FireEvent();
                    return true;

                case oui::VirtualKey::Escape:
                    if (auto menu = m_menuWindow.lock())
                    {
                        menu->Deactivate();
                    }
                    return true;
                }
            }
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
    static int ToString(const PopupItem& item, int fixedWidth, String& result)
    {
        const int g_spacesBefore = 2;
        const int g_spacesAfter = 2;

        result.native.clear();

        if (item.handler == nullptr)
        {
            // handler separator case here
            if (fixedWidth)
            {
                return fixedWidth;
            }
            return g_spacesBefore + g_spacesAfter;
        }

        int symCount = 0;
        // space before
        symCount += g_spacesBefore;
        result.native.append(g_spacesBefore, String::symSpace);

        // -- 
        symCount += CalculateSymbolsCount(item.text.native, g_HotKeySymbol);
        result.native += item.text.native;

        if (fixedWidth)
        {
            if (symCount < (fixedWidth - g_spacesAfter))
            {
                int padCount = fixedWidth - symCount - g_spacesAfter;
                result.native.append(padCount, String::symSpace);
            }
        }

        // space after
        symCount += g_spacesAfter;
        result.native.append(g_spacesAfter, String::symSpace);

        return symCount;
    }
    void CMenuPopup::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        // paint border
        Parent_type::DoPaint(rect, parameters);

        // paint body
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            return;
        }
        auto colorProfile = menu->GetColorProfile();

        Parent_type::SetColors(colorProfile->popup.borderColor, colorProfile->popup.borderBackgroundColor);

        const auto clientRect = GetClientRect();
        Point pos = clientRect.position + rect.position;
        String tmp;
        int index = 0;

        for (auto popup : *popupItems)
        {
            MenuButtonProfile* profile = &colorProfile->popup.normal;
            int symbolsCount = ToString(popup, clientRect.size.width, tmp);

            if (popup.handler == nullptr)
            {
                // handle separator
                auto sepPos = pos;
                --sepPos.x;
                parameters.console.PaintMenuSeparator(sepPos,
                    clientRect.size.width + 2,
                    colorProfile->popup.borderColor, 
                    colorProfile->popup.borderBackgroundColor,
                    BorderStyle::Thick);
            }
            else
            {
                if (index == m_selectedPosition)
                {
                    profile = &colorProfile->popup.selected;
                }
                parameters.console.PaintText(pos,
                    profile->buttonText,
                    profile->buttonBackground,
                    tmp,
                    g_HotKeySymbol,
                    profile->buttonHotkeyText,
                    profile->buttonBackground);
            }
            popup.text;
            ++pos.y;
            ++index;
        }
    }
    void CMenuPopup::OnFocusLost()
    {
        auto menu = m_menuWindow.lock();
        if (menu)
        {
            menu->DontSetFocusOnDeactivate();
        }
        Parent_type::OnFocusLost();
    }
    void CMenuPopup::UpdateHotkeys(std::shared_ptr<CWindow> menu,
        const std::vector<PopupItem>& items)
    {
        m_hotkeys.Clear();
        for (auto& item : items)
        {
            if (item.hotkey.hotkey != VirtualKey::None)
            {
                m_hotkeys.Register(item.hotkey,
                    [handler = item.handler, this, menu]() {
                        handler();
                        Destroy();
                });
            }
        }
    }
    void CMenuPopup::Dock()
    {
        m_selectedPosition = 0;
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            Destroy();
            menu->SetFocus();
            return;
        }
        UpdateHotkeys(menu, *popupItems);

        int maxWidth = 0;
        String tmp;
        for (auto popup: *popupItems)
        {
            int symbolsCount = ToString(popup, 0, tmp);
            if (symbolsCount > maxWidth)
            {
                maxWidth = symbolsCount;
            }
        }

        const auto borderSize = GetBorderSize(this);

        auto menuPosition = menu->GetPosition();
        auto buttonPosition = selectedButton->GetPosition();
        const Size size = { maxWidth + borderSize.width, (int)popupItems->size() + borderSize.height};

        const Point popupPosition = { buttonPosition.x, menuPosition.y + 1 };
        MoveTo(popupPosition);
        Resize(size);
        Invalidate();
    }

    // CMenuWindow
    CMenuWindow::CMenuWindow()
    {
        m_menuColorProfile = std::make_shared<MenuColorProfile>();
        QueryDefaultColorProfile(*m_menuColorProfile);
    }
    std::shared_ptr<CMenuButtonWindow> CMenuWindow::AddButton(const String& caption,
        std::function<void()> handler)
    {
        std::vector<PopupItem> empty;
        m_buttons.push_back(std::make_shared<CMenuButtonWindow>(caption, handler, std::move(empty)));
        return m_buttons.back();
    }
    std::shared_ptr<CMenuButtonWindow> CMenuWindow::AddButton(const String& caption,
        std::vector<PopupItem>&& items)
    {
        m_buttons.push_back(std::make_shared<CMenuButtonWindow>(caption, nullptr, std::move(items)));
        return m_buttons.back();
    }
    void CMenuWindow::ConstructChilds()
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
        Rect parentClientRect = parent->GetClientRect();
        if (parentClientRect.size.height <= 0)
        {
            return;
        }
        auto size = parentClientRect.size;
        size.height = 1;
        this->Resize(size);

        this->MoveTo(parentClientRect.position);

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

    std::shared_ptr<CMenuButtonWindow> CMenuWindow::GetSelectedButton()
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
    void CMenuWindow::ShiftSelectedButtonIndex(int difference)
    {
        if (m_buttons.empty())
        {
            m_selectedButtonIndex = 0;
            return;
        }
        int newIndex = m_selectedButtonIndex + difference;
        if (newIndex >= (int)m_buttons.size())
        {
            newIndex = 0;
        }
        else if (newIndex < 0)
        {
            newIndex = (int)m_buttons.size() - 1;
        }
        m_selectedButtonIndex = newIndex;
    }
    void CMenuWindow::SetPrevFocus(std::shared_ptr<CWindow> prevFocus)
    {
        m_prevFocus = prevFocus;
    }
    bool CMenuWindow::ProcessEvent(oui::InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.keyEvent.valid)
        {
            switch (evt.keyEvent.virtualKey)
            {
            case oui::VirtualKey::Escape:
                Deactivate();
                return true;
            case oui::VirtualKey::Left:
                ShiftSelectedButtonIndex(-1);
                Invalidate();
                return true;
            case oui::VirtualKey::Right:
                ShiftSelectedButtonIndex(1);
                Invalidate();
                return true;

            case oui::VirtualKey::Down:
            case oui::VirtualKey::Enter:
                // TODO: go to submenu
                this->OpenPopup();
                return true;
            }
        }
        return CWindow::ProcessEvent(evt, evtContext);
    }
    void CMenuWindow::Activate()
    {
        if (IsActive())
        {
            return;
        }
        CWindow::Activate();

        auto pool = GetPool();
        if (!pool)
        {
            return;
        }
        if (pool->GetFocus().get() != this)
        {
            SetPrevFocus(pool->GetFocus());
            SetFocus();
        }
    }
    void CMenuWindow::DontSetFocusOnDeactivate()
    {
        m_setFocusOnDeactivate = false;
    }
    void CMenuWindow::Deactivate()
    {
        auto pool = GetPool();
        if (m_currentPopup)
        {
            m_currentPopup->Detach();
            m_currentPopup->Destroy();
            m_currentPopup = nullptr;
        }
        if (m_setFocusOnDeactivate && pool)
        {
            pool->SetFocus(m_prevFocus.lock());
            m_prevFocus.reset();
        }
        CWindow::Deactivate();
    }
    void CMenuWindow::SelectAndOpenPopup(std::shared_ptr<CMenuButtonWindow> button)
    {
        auto it = std::find(m_buttons.begin(), m_buttons.end(), button);
        if (it == m_buttons.end())
        {
            return;
        }
        m_selectedButtonIndex = (int)(it - m_buttons.begin());
        Activate();
        OpenPopup();
    }
    bool CMenuWindow::PopupIsOpen() const 
    {
        return m_currentPopup.get();
    }
    void CMenuWindow::OnFocusLost()
    {
        if (!m_currentPopup)
        {
            if (IsActive())
            {
                Deactivate();
            }
        }
        Parent_type::OnFocusLost();
    }
    void CMenuWindow::OpenPopup()
    {
        if (m_currentPopup)
        {
            m_currentPopup->Detach();
            m_currentPopup->Destroy();
            m_currentPopup = nullptr;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = GetSelectedButton();
        if (!selectedButton)
        {
            return;
        }
        auto rootWindow = this->GetParent();
        if (!rootWindow)
        {
            return;
        }
        auto myPtr = Cast_t<CMenuWindow>(this->GetPtr());
        if (!myPtr)
        {
            return;
        }
        if (!selectedButton->GetPopupItems())
        {
            // no popups provided, call handler if any
            auto handler = selectedButton->GetHandler();
            if (handler)
            {
                Deactivate();
                handler();
            }
            return;
        }
        m_currentPopup = std::make_shared<CMenuPopup>(myPtr);
        rootWindow->AddChild(m_currentPopup);
        m_currentPopup->Init(rootWindow);
        m_currentPopup->Dock();
        m_currentPopup->SetFocus();
    }
}