#include "oui_menu.h"

namespace oui
{
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
    bool CMenuButtonWindow::HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)
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
        bool treatAsMove = (evt.mouseEvent.button != MouseButton::Left ||
            evt.mouseEvent.state != MouseState::Released);

        bool enterState = false;
        if (treatAsMove)
        {
            if (!parentMenu->IsActiveOrFocused())
            {
                // if just move, return, if presses activate fast
                if (evt.mouseEvent.state != MouseState::Pressed)
                {
                    return true;
                }
                enterState = true;
            }
        }
        else
        {
            // check if already open
            bool wasEnterState = parentMenu->ClearEnterState(me);
            if (!wasEnterState)
            {
                if (parentMenu->GetSelectedButton().get() == this &&
                    parentMenu->PopupIsOpen())
                {
                    parentMenu->Deactivate();
                    return true;
                }
            }
        }
        parentMenu->SelectAndOpenPopup(me, enterState);
        return true;
    }
    void CMenuButtonWindow::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        String hotKeySymbol = GetHotKeySymbol();

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
            hotKeySymbol,
            profile->buttonHotkeyText,
            profile->buttonBackground);
    }
    void CMenuButtonWindow::Dock()
    {
        String hotKeySymbol = GetHotKeySymbol();

        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        int symbols = console->GetSymbolsAnalyzer().CalculateSymbolsCount(m_caption.native, hotKeySymbol.native[0]);
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
        Parent_type(false),
        m_menuWindow(menuWindow)
    {
    }
    CMenuPopup::CMenuPopup(std::vector<PopupItem>&& items, std::shared_ptr<MenuColorProfile> menuColorProfile)
        :
        Parent_type(false),
        m_items(std::move(items)),
        m_menuColorProfile(menuColorProfile)
    {
        if (!menuColorProfile)
        {
            m_menuColorProfile = std::make_shared<MenuColorProfile>();
            QueryDefaultColorProfile(*m_menuColorProfile);
        }
    }
    void CMenuPopup::Detach()
    {
        m_menuWindow.reset();
    }
    void CMenuPopup::Destroy()
    {
        auto guard = GetPtr();
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
    bool CMenuPopup::HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)
    {
        auto popupItems = GetPopupItems();
        if (!popupItems)
        {
            return false;
        }

        auto relativePoint = GetClientMousePoint(mouseEventContext, this, rect, evt.mouseEvent.point);
        if (relativePoint.x < 0 || relativePoint.y < 0 || relativePoint.y >= (int)popupItems->size())
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
        case MouseButton::Left:
            if (evt.mouseEvent.state == MouseState::Released)
            {
                FireEvent();
                break;
            }
            // go to move
        case MouseButton::Move:
            m_selectedPosition = index;
            Invalidate();
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
        auto popupItems = GetPopupItems();
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

        if (m_hotkeys.ProcessEvent(evt))
        {
            return true;
        }
        if (evt.keyEvent.valid)
        {
            switch (evt.keyEvent.virtualKey)
            {
            case oui::VirtualKey::Left:
                if (parentMenu)
                {
                    parentMenu->ShiftSelectedButtonIndex(-1);
                    Dock();
                    return true;
                }
                break;
            case oui::VirtualKey::Right:
                if (parentMenu)
                {
                    parentMenu->ShiftSelectedButtonIndex(1);
                    Dock();
                    return true;
                }
                break;
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
                else
                {
                    Destroy();
                }
                return true;
            }
        }

        return Parent_type::ProcessEvent(evt, evtContext);
    }
    static int ToString(CConsole * console, const PopupItem& item, int fixedWidth, String& result)
    {
        String hotKeySymbol = GetHotKeySymbol();

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
        symCount += console->GetSymbolsAnalyzer().CalculateSymbolsCount(item.text.native, hotKeySymbol.native[0]);
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
    std::shared_ptr<MenuColorProfile> CMenuPopup::GetColorProfile()
    {
        if (m_menuColorProfile)
        {
            return m_menuColorProfile;
        }
        auto menu = m_menuWindow.lock();
        if (!menu)
        {
            return nullptr;
        }
        std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
        if (!selectedButton)
        {
            return nullptr;
        }
        auto popupItems = selectedButton->GetPopupItems();
        if (!popupItems)
        {
            return nullptr;
        }
        return menu->GetColorProfile();
    }
    std::shared_ptr<const std::vector<PopupItem>> CMenuPopup::GetPopupItems()
    {
        auto menu = m_menuWindow.lock();
        if (menu)
        {
            std::shared_ptr<CMenuButtonWindow> selectedButton = menu->GetSelectedButton();
            if (selectedButton)
            {
                return selectedButton->GetPopupItems();
            }
        }
        auto me = this->GetPtr();
        if (!me)
        {
            return nullptr;
        }
        return std::shared_ptr< const std::vector<PopupItem>>(me, &m_items);
    }
    void CMenuPopup::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        String hotKeySymbol = GetHotKeySymbol();

        auto console = GetConsole();
        if (!console)
        {
            return;
        }

        // paint rect
        auto colorProfile = GetColorProfile();
        if (!colorProfile)
        {
            return;
        }
        Parent_type::SetColors(colorProfile->popup.borderColor, colorProfile->popup.borderBackgroundColor);
        Parent_type::DoPaint(rect, parameters);


        // paint body
        auto popupItems = GetPopupItems();
        if (!popupItems)
        {
            return;
        }
        const auto clientRect = GetClientRect();
        Point pos = clientRect.position + rect.position;
        String tmp;
        int index = 0;

        for (auto popup : *popupItems)
        {
            MenuButtonProfile* profile = &colorProfile->popup.normal;
            int symbolsCount = ToString(console, popup, clientRect.size.width, tmp);

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
                    hotKeySymbol,
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
        if (IsDestroyed() || m_destroyed || m_dialogFinished)
        {
            return;
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
    void CMenuPopup::DockImpl(const std::vector<PopupItem>& items, const Point & popupPosition)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        int maxWidth = 0;
        String tmp;
        for (auto popup : items)
        {
            int symbolsCount = ToString(console, popup, 0, tmp);
            if (symbolsCount > maxWidth)
            {
                maxWidth = symbolsCount;
            }
        }

        // Yep, I know this is a hardcode, but what are you gonna do
        auto borderSize = Size{ 2, 2 };
        if (GetBorderStyle() == BorderStyle::None)
        {
            borderSize = Size{ 0, 0 };
        }

        const Size size = { maxWidth + borderSize.width, (int)items.size() + borderSize.height };

        MoveTo(popupPosition);
        Resize(size);
        Invalidate();
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
        auto menuPosition = menu->GetPosition();
        auto buttonPosition = selectedButton->GetPosition();

        UpdateHotkeys(menu, *popupItems);

        const Point popupPosition = { buttonPosition.x, menuPosition.y + 1 };
        DockImpl(*popupItems, popupPosition);
    }
    void CMenuPopup::Dock(const Point& popupPosition)
    {
        DockImpl(m_items, popupPosition);
    }

    std::shared_ptr<CWindow> CMenuPopup::GetPopupPrevFocusTarget()
    {
        auto menu = m_menuWindow.lock();
        if (menu)
        {
            return menu->GetPopupPrevFocusTarget();
        }
        return nullptr;
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
        if (m_selectedButtonIndex < 0 || m_selectedButtonIndex >= (int)m_buttons.size())
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
    std::shared_ptr<CWindow> CMenuWindow::GetPopupPrevFocusTarget()
    {
        return m_prevFocus.lock();
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
    bool CMenuWindow::ClearEnterState(std::shared_ptr<CMenuButtonWindow> button)
    {
        if (m_enterStateIndex == -1)
        {
            return false;
        }
        auto it = std::find(m_buttons.begin(), m_buttons.end(), button);
        if (it == m_buttons.end())
        {
            return false;
        }
        auto selectedButtonIndex = (int)(it - m_buttons.begin());
        bool prevState = selectedButtonIndex == m_enterStateIndex;
        m_enterStateIndex = -1;
        return prevState;
    }
    void CMenuWindow::SelectAndOpenPopup(std::shared_ptr<CMenuButtonWindow> button,
        bool enterState)
    {
        auto it = std::find(m_buttons.begin(), m_buttons.end(), button);
        if (it == m_buttons.end())
        {
            return;
        }
        m_selectedButtonIndex = (int)(it - m_buttons.begin());

        if (enterState)
        {
            m_enterStateIndex = m_selectedButtonIndex;
        }
        else
        {
            if (m_enterStateIndex == m_selectedButtonIndex)
            {
                return;
            }
            m_enterStateIndex = -1;
        }
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
        m_setFocusOnDeactivate = true;
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