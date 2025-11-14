#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_modules_window.h"
#include <ctime>

oui::String NameInfoFlagsToString(int flags)
{
    if (flags & orthia::NameInfo::flags_Import)
    {
        return OUI_TCHAR("I");
    }
    if (flags & orthia::NameInfo::flags_Export)
    {
        return OUI_TCHAR("E");
    }
    return OUI_TCHAR("F");
}

CModulesWindow::CModulesWindow(std::function<oui::String()> getCaption, 
    std::shared_ptr<orthia::CProgramModel> model,
    std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher,
    std::function<void(orthia::Address_type address)> onGotoAddress)
    : 
    ParentType(getCaption),
    m_model(model),
    m_onGotoAddress(onGotoAddress)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());

    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    // create modules edit box
    auto columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.modules.columns"));

    m_modulesOwner.getTotalCount = [this]() { return Modules_GetTotalCount(); };
    m_modulesOwner.shiftViewWindow = [this](int newOffset) { return Modules_ShiftViewWindow(newOffset); };

    m_modulesBox = std::make_shared<oui::CListBox>(m_colorProfile, &m_modulesOwner);
    m_modulesBox->InitColumns(oui::ColumnParam([=] { return columnsNode->QueryValue(L"name");  }, 25),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"address");  }, 19, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"mapped-size");  }, 11, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"full-path");  }, 55, oui::ColumnFormat::ctLeft)

    );
    m_modulesBox->SetBorderStyle(oui::BorderStyle::None);
    m_modulesBox->Dock();

    m_modulesScrollable = std::make_shared<oui::CScrollable>(m_modulesBox);

    // m_verticalScroll
    m_verticalScroll = std::make_shared<oui::CVerticalScrollBar>();

    // create names 
    columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.names.columns"));

    m_namesOwner.getTotalCount = [this]() { return Names_GetTotalCount(); };
    m_namesOwner.shiftViewWindow = [this](int newOffset) { return Names_ShiftViewWindow(newOffset); };

    m_namesBox = std::make_shared<oui::CListBox>(m_colorProfile, &m_namesOwner);
    m_namesBox->InitColumns(
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"type");  }, 8, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"name");  }, 60),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"address");  }, 19, oui::ColumnFormat::ctCenter)
    );
    m_namesBox->SetBorderStyle(oui::BorderStyle::None);
    m_namesBox->Dock();

    m_namesScrollable = std::make_shared<oui::CScrollable>(m_namesBox);

    auto labelProfile = std::shared_ptr<oui::LabelColorProfile>(m_colorProfile, &m_colorProfile->label);
    m_namesModuleLabel = std::make_shared<oui::CLabel>(labelProfile, [this]() { return GetCurrentSelectedModule();  });

    RegisterSwitch(m_modulesBox);
    RegisterSwitch(m_namesBox);
    RegisterSwitchParent(parentTabSwitcher);

    m_verticalScroll->SetDragHandler([this](const oui::Point& point) { VertScroll_OnStartDrag(point); });
}

void CModulesWindow::VertScroll_OnStartDrag(const oui::Point& point)
{
    struct ResizeState
    {
        int m_initialModulesWidthPercent = 0;
        oui::Rect modulesRect;
    };
    ResizeState resiseState;
    resiseState.m_initialModulesWidthPercent = 0;
    resiseState.modulesRect = CalcModulesScrollRect(GetClientRect());
    m_verticalScroll->RegisterDragEvent(point, [this, originalState = resiseState](oui::DragEvent evt,
        const oui::Point& initialPoint,
        const oui::Point& currentPoint,
        std::shared_ptr<CWindow> wnd) {
        switch (evt)
        {
        default:
            return false;

        case oui::DragEvent::Progress:
        case oui::DragEvent::Drop:
        {
            int differenceX = currentPoint.x - initialPoint.x;

            const auto clientRect = GetClientRect();
            if (clientRect.size.width && differenceX)
            {
                int newWidth = originalState.modulesRect.size.width + differenceX;
                int diffInPercentage = (newWidth * 100) / clientRect.size.width;
                m_modulesWidthPercent = diffInPercentage;
            }
        }

        break;
        case oui::DragEvent::Cancel:
            m_modulesWidthPercent = originalState.m_initialModulesWidthPercent;
        }
        ForceResize();
        return true;
    });
}

bool CModulesWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) 
{
    if (evt.keyState.state & evt.keyState.AnyCtrl)
    {
        if (evt.keyEvent.valid && evt.keyEvent.virtualKey == oui::VirtualKey::Tab)
        {
            if (auto parent = ChildSwitcher_GetParent())
            {
                return parent->SwitchNextPanel();
            }
        }
    }
    return ParentType::ProcessEvent(evt, evtContext);
}

oui::String CModulesWindow::GetCurrentSelectedModule() const
{
    return m_selectedModuleName;
}
void CModulesWindow::SelectModule(orthia::Address_type address, const oui::String& name)
{
    m_selectedModuleAddress = address;
    m_selectedModuleName = name;
    m_cachedNamesPage.clear();
    m_lastTotalNamesCount = 0;
    UpdateVisibleItems();
    Invalidate();
}
int CModulesWindow::Modules_GetTotalCount() const
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        return activeItem->GetModulesCount();
    }
    return 0;
}
int CModulesWindow::Names_GetTotalCount() const
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        return activeItem->QueryNamesCount(m_selectedModuleAddress, orthia::NameSelectionKey());
    }
    return 0;
}
void CModulesWindow::UpdateVisibleItems()
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> modules;
    if (activeItem)
    {
        activeItem->GetModules(modules);
    }
    auto firstRunAfterReload = m_needUpdateModulesBox;
    if (firstRunAfterReload)
    {
        m_needUpdateModulesBox = false;
        m_modulesBox->SetOffset(m_requiredModulesBoxOffset);
        m_modulesBox->SetSelectedPosition(m_requiredModulesBoxPosition);

        m_requiredModulesBoxOffset = 0;
        m_requiredModulesBoxPosition = 0;
    }

    DefaultUpdateVisibleItems(this, &m_modulesOwner, m_modulesBox, modules,
        [&](auto it, auto vit)
    {
        vit->text.clear();
        vit->text.push_back(it->name);
        vit->text.push_back(orthia::ToWideStringAsHex(it->address));
        vit->text.push_back(orthia::ToWideStringAsHex((unsigned int)it->size));
        vit->text.push_back(it->fullName);

        vit->openHandler = [this, address = it->address, name = it->name] {
            SelectModule(address, name);
        };
        vit->colorsHandler = [=]() { return oui::LabelColorState{ m_colorProfile->listBoxFolders, oui::Color() }; };
    });

    if (firstRunAfterReload)
    {
        // first run after reload state
        activeItem->QueryNames(m_selectedModuleAddress, orthia::NameSelectionKey(), m_requiredNamesCacheSize, m_cachedNamesPage);
        m_namesBox->SetOffset(m_requiredNamesOffset);
        m_namesBox->SetSelectedPosition(0);

        m_requiredNamesOffset = 0;
        m_requiredNamesCacheSize = 0;
    }
    else
    if (m_cachedNamesPage.empty() && m_selectedModuleAddress)
    {
        // usual first run
        m_namesBox->SetSelectedPosition(0);
        activeItem->QueryNames(m_selectedModuleAddress, orthia::NameSelectionKey(), g_nameCacheSize, m_cachedNamesPage);
    }
    else if (!m_cachedNamesPage.empty())
    {
        // need next page
        auto offset = m_namesBox->GetOffset();
        auto visibleSize = m_namesBox->GetVisibleSize();
        auto itemsHandled = offset + visibleSize;
        if (itemsHandled > m_cachedNamesPage.size() && itemsHandled < m_lastTotalNamesCount)
        {
            // reload more data
            orthia::NameSelectionKey key;
            key.flags = key.flags_ContinueFrom;
            key.address = m_cachedNamesPage.back().address;

            std::vector<orthia::NameInfo> newPage;
            activeItem->QueryNames(m_selectedModuleAddress, key, g_nameCacheSize, newPage);
            m_cachedNamesPage.insert(m_cachedNamesPage.end(), newPage.begin(), newPage.end());
        }
    }
    DefaultUpdateVisibleItems(this, &m_namesOwner, m_namesBox, m_cachedNamesPage,
        [&](auto it, auto vit)
    {

        vit->text.clear();
        vit->text.push_back(NameInfoFlagsToString(it->flags));
        vit->text.push_back(it->name);
        vit->text.push_back(orthia::ToWideStringAsHex(it->address));

        vit->openHandler = [this, address = it->address]() {
            GotoAddress(address);
        };
        vit->colorsHandler = [=]() { return oui::LabelColorState{ m_colorProfile->listBoxFolders, oui::Color() }; };
    });
}
void CModulesWindow::GotoAddress(orthia::Address_type address)
{
    m_onGotoAddress(address);
}
void CModulesWindow::Modules_ShiftViewWindow(int newOffset)
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        activeItem->GetModules(items);
    }
 
    DefaultShiftViewWindow(m_modulesBox, newOffset, items.size());
    UpdateVisibleItems();
}
void CModulesWindow::Names_ShiftViewWindow(int newOffset)
{
    m_namesBox->SetOffset(newOffset);
    auto count = Names_GetTotalCount();
    m_lastTotalNamesCount = count;
    DefaultShiftViewWindow(m_namesBox, newOffset, count);
    UpdateVisibleItems();
}
void CModulesWindow::ConstructChilds()
{
    AddChild(m_modulesScrollable);
    AddChild(m_verticalScroll);
    AddChild(m_namesScrollable);
    AddChild(m_namesModuleLabel);
}
oui::Rect CModulesWindow::CalcModulesScrollRect(const oui::Rect& clientRect)
{
    oui::Rect scrollRect = clientRect;
    OPERAND_SIZE tmp = scrollRect.size.width;
    tmp *= m_modulesWidthPercent;
    tmp /= 100;
    scrollRect.size.width = (int)tmp;
    return scrollRect;
}
void CModulesWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    oui::Rect scrollRect = CalcModulesScrollRect(clientRect);
    m_modulesScrollable->Resize(scrollRect.size);

    m_verticalScroll->MoveTo(oui::Point(scrollRect.size.width, 0));
    m_verticalScroll->Resize(clientRect.size);

    oui::Rect namesScrollRect;
    namesScrollRect.position.x = scrollRect.size.width + 1;
    namesScrollRect.position.y = 1;
    namesScrollRect.size.width = clientRect.size.width - namesScrollRect.position.x;
    namesScrollRect.size.height = clientRect.size.height - 1;
    m_namesScrollable->MoveTo(namesScrollRect.position);
    m_namesScrollable->Resize(namesScrollRect.size);

    auto labelPosition = namesScrollRect.position;
    ++labelPosition.x;
    labelPosition.y = 0;
    m_namesModuleLabel->MoveTo(labelPosition);
    auto labelSize = namesScrollRect.size;
    labelSize.width -= 2;
    labelSize.height = 1;
    m_namesModuleLabel->Resize(labelSize);

    UpdateVisibleItems();
}
void CModulesWindow::SetFocusImpl()
{
    UpdateVisibleItems();
    m_modulesScrollable->SetFocus();
}
void CModulesWindow::OnWorkspaceItemChanged()
{
    UpdateVisibleItems();
}
void CModulesWindow::SetActiveWorkspaceItem(int itemId)
{
    OnWorkspaceItemChanged();
}
void CModulesWindow::Invalidate(bool valid)
{
    ParentType::Invalidate(valid);
}
void CModulesWindow::ReloadState(const UIState& state) 
{
    Apply(state.addresses, field_selectedModuleAddress, 
        [&](auto& value) { m_selectedModuleAddress = value; },
        [&]() { m_selectedModuleAddress = 0; }
    );
    Apply(state.addresses, field_namesBox_Offset, 
        [&](auto& value) { m_requiredNamesOffset = (int)value; },
        [&]() { m_requiredNamesOffset = 0; }
    );
    Apply(state.addresses, field_cachedNamesPage_size, 
        [&](auto& value) { m_requiredNamesCacheSize = (int)value; },
        [&]() { m_requiredNamesCacheSize = 0; }
    );
    Apply(state.strings, field_selectedModuleName, 
        [&](auto& value) { m_selectedModuleName = value; },
        [&]() { m_selectedModuleName.native.clear(); }
    );
    Apply(state.addresses, field_modulesBox_Offset,
        [&](auto& value) { m_requiredModulesBoxOffset = (int)value; },
        [&]() { m_requiredModulesBoxOffset = 0; }
    );
    Apply(state.addresses, field_modulesBox_Position,
        [&](auto& value) { m_requiredModulesBoxPosition = (int)value; },
        [&]() { m_requiredModulesBoxPosition = 0; }
    );
    m_needUpdateModulesBox = true;
    Invalidate();
}

void CModulesWindow::SaveState(UIState& state)
{
    state.addresses[field_selectedModuleAddress] = m_selectedModuleAddress;
    state.addresses[field_namesBox_Offset] = m_namesBox->GetOffset();
    state.addresses[field_cachedNamesPage_size] = m_cachedNamesPage.size();
    state.strings[field_selectedModuleName] = m_selectedModuleName;

    if (m_needUpdateModulesBox)
    {
        state.addresses[field_modulesBox_Offset] = m_requiredModulesBoxOffset;
        state.addresses[field_modulesBox_Position] = m_requiredModulesBoxPosition;
    }
    else
    {
        state.addresses[field_modulesBox_Offset] = m_modulesBox->GetOffset();
        state.addresses[field_modulesBox_Position] = m_modulesBox->GetSelectedPosition();
    }
}
