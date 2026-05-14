#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_sections_window.h"
#include "orthia_model.h"
#include "oui_menu.h"

CSectionsWindow::CSectionsWindow(std::function<oui::String()> getCaption,
                                 std::shared_ptr<orthia::CProgramModel> model,
                                 std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher)
    :
    ParentType(getCaption),
    m_model(model)
{
    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    auto labelProfile = std::shared_ptr<oui::LabelColorProfile>(m_colorProfile, &m_colorProfile->label);

    m_headerLabel = std::make_shared<oui::CLabel>(labelProfile, [] {
        return oui::String(OUI_TCHAR("Module: "));
    });

    m_addressEdit = std::make_shared<oui::CEditBox>(m_colorProfile);
    m_addressEdit->SetEnterHandler([this](const oui::String&) {
        UpdateSections();
    });

    // sections list (left pane)
    auto sectionsColumnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.sections.columns"));

    m_sectionsOwner.getTotalCount      = [this]() { return Sections_GetTotalCount(); };
    m_sectionsOwner.shiftViewWindow    = [this](int off) { Sections_ShiftViewWindow(off); };
    m_sectionsOwner.onVisibleItemChanged = [this]() { Sections_OnVisibleItemChanged(); };

    m_sectionsBox = std::make_shared<oui::CListBox>(m_colorProfile, &m_sectionsOwner);
    m_sectionsBox->InitColumns(
        oui::ColumnParam([=] { return sectionsColumnsNode->QueryValue(ORTHIA_TCSTR("name")); },   20),
        oui::ColumnParam([=] { return sectionsColumnsNode->QueryValue(ORTHIA_TCSTR("address")); }, 19, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return sectionsColumnsNode->QueryValue(ORTHIA_TCSTR("size")); },    16, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return sectionsColumnsNode->QueryValue(ORTHIA_TCSTR("flags")); },    8, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return ORTHIA_TCSTR(""); }, 100, oui::ColumnFormat::ctCenter)
    );
    m_sectionsBox->SetBorderStyle(oui::BorderStyle::None);
    m_sectionsBox->Dock();
    m_sectionsScrollable = std::make_shared<oui::CScrollable>(m_sectionsBox);

    m_verticalScroll = std::make_shared<oui::CVerticalScrollBar>();
    m_verticalScroll->SetDragHandler([this](const oui::Point& point) { VertScroll_OnStartDrag(point); });

    // attributes list (right pane)
    auto attrsColumnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.sections.attrs"));

    m_attrsOwner.getTotalCount   = [this]() { return Attrs_GetTotalCount(); };
    m_attrsOwner.shiftViewWindow = [this](int off) { Attrs_ShiftViewWindow(off); };

    m_attrsBox = std::make_shared<oui::CListBox>(m_colorProfile, &m_attrsOwner);
    m_attrsBox->InitColumns(
        oui::ColumnParam([=] { return attrsColumnsNode->QueryValue(ORTHIA_TCSTR("field")); }, 30),
        oui::ColumnParam([=] { return attrsColumnsNode->QueryValue(ORTHIA_TCSTR("value")); }, 70)
    );
    m_attrsBox->SetBorderStyle(oui::BorderStyle::None);
    m_attrsBox->Dock();
    m_attrsScrollable = std::make_shared<oui::CScrollable>(m_attrsBox);

    RegisterSwitch(m_addressEdit);
    RegisterSwitch(m_sectionsBox);
    RegisterSwitch(m_attrsBox);
    RegisterSwitchParent(parentTabSwitcher);
}

void CSectionsWindow::VertScroll_OnStartDrag(const oui::Point& point)
{
    struct ResizeState { int initialPercent = 0; oui::Rect sectionsRect; };
    ResizeState state;
    state.initialPercent = m_sectionsWidthPercent;
    state.sectionsRect   = CalcSectionsRect(GetClientRect());

    m_verticalScroll->RegisterDragEvent(point, [this, state](oui::DragEvent evt,
        const oui::Point& initialPoint,
        const oui::Point& currentPoint,
        std::shared_ptr<oui::CWindow>)
    {
        switch (evt)
        {
        case oui::DragEvent::Progress:
        case oui::DragEvent::Drop:
        {
            int dx = currentPoint.x - initialPoint.x;
            const auto cr = GetClientRect();
            if (cr.size.width && dx)
            {
                int newW = state.sectionsRect.size.width + dx;
                m_sectionsWidthPercent = (newW * 100) / cr.size.width;
            }
            break;
        }
        case oui::DragEvent::Cancel:
            m_sectionsWidthPercent = state.initialPercent;
            break;
        default:
            return false;
        }
        ForceResize();
        return true;
    });
}

oui::Rect CSectionsWindow::CalcSectionsRect(const oui::Rect& clientRect) const
{
    oui::Rect r = clientRect;
    OPERAND_SIZE tmp = r.size.width;
    tmp *= m_sectionsWidthPercent;
    tmp /= 100;
    r.size.width = (int)tmp;
    return r;
}

int CSectionsWindow::Sections_GetTotalCount() const
{
    return (int)m_cachedSections.size();
}
void CSectionsWindow::Sections_ShiftViewWindow(int newOffset)
{
    DefaultShiftViewWindow(m_sectionsBox, newOffset, m_cachedSections.size());
    UpdateSectionsVisibleItems();
}
void CSectionsWindow::Sections_OnVisibleItemChanged()
{
    int idx = m_sectionsBox->GetOffset() + m_sectionsBox->GetSelectedPosition();
    if (idx >= 0 && idx < (int)m_cachedSections.size())
    {
        m_selectedSection = &m_cachedSections[idx];
    }
    else
    {
        m_selectedSection = nullptr;
    }
    m_attrsBox->SetOffset(0);
    m_attrsBox->SetSelectedPosition(0);
    UpdateAttrsVisibleItems();
    Invalidate();
}

int CSectionsWindow::Attrs_GetTotalCount() const
{
    return m_selectedSection ? (int)m_selectedSection->attributes.size() : 0;
}
void CSectionsWindow::Attrs_ShiftViewWindow(int newOffset)
{
    if (m_selectedSection)
    {
        DefaultShiftViewWindow(m_attrsBox, newOffset, m_selectedSection->attributes.size());
    }
    UpdateAttrsVisibleItems();
}

void CSectionsWindow::UpdateSections()
{
    m_cachedSections.clear();
    m_selectedSection = nullptr;
    m_sectionsBox->Clear();
    m_attrsBox->Clear();

    auto activeItem = m_model->GetActiveItem();
    if (!activeItem)
        return;

    oui::String input = m_addressEdit->GetText();
    if (input.native.empty())
        return;
    
    orthia::Address_type address = 0;
    bool addressFound = false;

    try
    {
        address = oui::CaptureAddress(input.native);
        addressFound = true;
    }
    catch (std::exception&)
    {
    }
    if (!addressFound)
    {
        oui::EnumModulesByName(activeItem, input.native,
            [&](orthia::ModuleInfo& mod)
        {
            addressFound = true;
            address = mod.address;
            return false;
        });
        if (!addressFound)
        {
            return;
        }
    }
    activeItem->QuerySections(address, m_cachedSections);

    m_sectionsBox->SetOffset(0);
    m_sectionsBox->SetSelectedPosition(0);
    if (!m_cachedSections.empty())
    {
        m_selectedSection = &m_cachedSections[0];
    }
    UpdateSectionsVisibleItems();
    UpdateAttrsVisibleItems();
    Invalidate();
}

void CSectionsWindow::UpdateSectionsVisibleItems()
{
    DefaultUpdateVisibleItems(this, &m_sectionsOwner, m_sectionsBox, m_cachedSections,
        [&](auto it, auto vit)
        {
            vit->text.clear();
            vit->text.push_back(it->name);
            vit->text.push_back(orthia::ToWideStringAsHex(it->virtualAddress));
            vit->text.push_back(orthia::ToWideStringAsHex(it->size));
            vit->text.push_back(it->flagsShort);

            vit->openHandler = [this, &sec = *it]() {
                m_selectedSection = &sec;
                m_attrsBox->SetOffset(0);
                m_attrsBox->SetSelectedPosition(0);
                UpdateAttrsVisibleItems();
                Invalidate();
            };
            vit->colorsHandler = [=]() {
                return oui::LabelColorState{ m_colorProfile->listBoxFolders, oui::Color() };
            };
        });
}

void CSectionsWindow::UpdateAttrsVisibleItems()
{
    if (!m_selectedSection)
    {
        m_attrsBox->Clear();
        return;
    }
    DefaultUpdateVisibleItems(this, &m_attrsOwner, m_attrsBox, m_selectedSection->attributes,
        [&](auto it, auto vit)
        {
            vit->text.clear();
            vit->text.push_back(it->first);
            vit->text.push_back(it->second);
            vit->colorsHandler = [=]() {
                return oui::LabelColorState{ m_colorProfile->listBoxFolders, oui::Color() };
            };
        });
}

void CSectionsWindow::NavigateTo(orthia::Address_type moduleBase, const oui::String& displayName)
{
    m_addressEdit->SetText(displayName);

    m_cachedSections.clear();
    m_selectedSection = nullptr;

    auto activeItem = m_model->GetActiveItem();
    if (activeItem && moduleBase)
    {
        activeItem->QuerySections(moduleBase, m_cachedSections);
        m_sectionsBox->SetOffset(0);
        m_sectionsBox->SetSelectedPosition(0);
        if (!m_cachedSections.empty())
            m_selectedSection = &m_cachedSections[0];
    }

    UpdateSectionsVisibleItems();
    UpdateAttrsVisibleItems();
    Invalidate();
}

void CSectionsWindow::ConstructChilds()
{
    AddChild(m_headerLabel);
    AddChild(m_addressEdit);
    AddChild(m_sectionsScrollable);
    AddChild(m_verticalScroll);
    AddChild(m_attrsScrollable);
}

void CSectionsWindow::OnResize()
{
    const oui::Rect cr = GetClientRect();

    // top bar: label + editbox on row 0
    const int labelWidth = 9;  // "Module: " length
    m_headerLabel->MoveTo({ 0, 0 });
    m_headerLabel->Resize({ labelWidth, 1 });

    oui::Rect editRect;
    editRect.position = { labelWidth, 0 };
    editRect.size     = { cr.size.width - labelWidth, 1 };
    m_addressEdit->MoveTo(editRect.position);
    m_addressEdit->Resize(editRect.size);

    // sections list (left), splitter, attrs (right) — below row 0
    oui::Rect body = cr;
    body.position.y = 1;
    body.size.height = cr.size.height - 1;

    oui::Rect secRect = CalcSectionsRect(body);
    secRect.position.y = 1;
    m_sectionsScrollable->MoveTo({ 0, 1 });
    m_sectionsScrollable->Resize(secRect.size);

    m_verticalScroll->MoveTo({ secRect.size.width, 1 });
    m_verticalScroll->Resize({ 1, body.size.height });

    oui::Rect attrsRect;
    attrsRect.position.x = secRect.size.width + 1;
    attrsRect.position.y = 1;
    attrsRect.size.width  = cr.size.width - attrsRect.position.x;
    attrsRect.size.height = body.size.height;
    m_attrsScrollable->MoveTo(attrsRect.position);
    m_attrsScrollable->Resize(attrsRect.size);

    UpdateSectionsVisibleItems();
    UpdateAttrsVisibleItems();
}

void CSectionsWindow::SetFocusImpl()
{
    m_addressEdit->SetFocus();
}

bool CSectionsWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext)
{
    if (evt.keyState.state & evt.keyState.AnyCtrl)
    {
        if (evt.keyEvent.valid && evt.keyEvent.virtualKey == oui::VirtualKey::Tab)
        {
            if (auto parent = ChildSwitcher_GetParent())
                return parent->SwitchNextPanel();
        }
    }
    return ParentType::ProcessEvent(evt, evtContext);
}

void CSectionsWindow::SetActiveWorkspaceItem(int /*itemId*/)
{
    m_cachedSections.clear();
    m_selectedSection = nullptr;
    UpdateSectionsVisibleItems();
    UpdateAttrsVisibleItems();
    Invalidate();
}

void CSectionsWindow::Invalidate(bool valid)
{
    ParentType::Invalidate(valid);
}

void CSectionsWindow::ReloadState(const UIState& state)
{
    oui::String address;
    Apply(state.strings, field_moduleAddress,
        [&](auto& v) { address = v; },
        [&]() { address.native.clear(); });

    int sectionsOffset = 0, sectionsPosition = 0, attrsOffset = 0;
    Apply(state.addresses, field_sectionsBox_Offset,
        [&](auto& v) { sectionsOffset = (int)v; },   [&]() {});
    Apply(state.addresses, field_sectionsBox_Position,
        [&](auto& v) { sectionsPosition = (int)v; }, [&]() {});
    Apply(state.addresses, field_attrsBox_Offset,
        [&](auto& v) { attrsOffset = (int)v; },      [&]() {});

    m_addressEdit->SetText(address);
    UpdateSections();

    if (sectionsOffset != 0 || sectionsPosition != 0 || attrsOffset != 0)
    {
        m_sectionsBox->SetOffset(sectionsOffset);
        m_sectionsBox->SetSelectedPosition(sectionsPosition);
        int idx = sectionsOffset + sectionsPosition;
        m_selectedSection = (idx >= 0 && idx < (int)m_cachedSections.size())
                            ? &m_cachedSections[idx] : nullptr;
        m_attrsBox->SetOffset(attrsOffset);
        m_attrsBox->SetSelectedPosition(0);
        UpdateSectionsVisibleItems();
        UpdateAttrsVisibleItems();
        Invalidate();
    }
}

void CSectionsWindow::SaveState(UIState& state)
{
    state.strings[field_moduleAddress]          = m_addressEdit->GetText();
    state.addresses[field_sectionsBox_Offset]   = m_sectionsBox->GetOffset();
    state.addresses[field_sectionsBox_Position] = m_sectionsBox->GetSelectedPosition();
    state.addresses[field_attrsBox_Offset]      = m_attrsBox->GetOffset();
}
