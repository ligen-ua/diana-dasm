#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_modules_window.h"
#include <ctime>

CModulesWindow::CModulesWindow(std::function<oui::String()> getCaption, std::shared_ptr<orthia::CProgramModel> model)
    : 
     oui::SimpleBrush<oui::CPanelWindow>(getCaption),
     m_model(model)
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

    // create names edit box
    columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.names.columns"));

    m_namesBox = std::make_shared<oui::CListBox>(m_colorProfile, &m_namesOwner);
    m_namesBox->InitColumns(oui::ColumnParam([=] { return columnsNode->QueryValue(L"name");  }, 50),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"address");  }, 19, oui::ColumnFormat::ctCenter),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"type");  }, 11, oui::ColumnFormat::ctCenter)

    );
    m_namesBox->SetBorderStyle(oui::BorderStyle::None);
    m_namesBox->Dock();

    m_namesScrollable = std::make_shared<oui::CScrollable>(m_namesBox);
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
void CModulesWindow::UpdateVisibleItems()
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> modules;
    if (activeItem)
    {
        activeItem->GetModules(modules);
    }
    
    DefaultUpdateVisibleItems(this, &m_modulesOwner, m_modulesBox, modules,
        [&](auto it, auto vit)
    {
        std::wstring name;
        orthia::UnparseFileNameFromFullFileName(it->fullName, &name);

        vit->text.clear();
        vit->text.push_back(name);
        vit->text.push_back(orthia::ToWideStringAsHex(it->address));
        vit->text.push_back(orthia::ToWideStringAsHex((unsigned int)it->size));
        vit->text.push_back(it->fullName);

        vit->openHandler = []() {
        };
        vit->colorsHandler = nullptr;
    });

    std::vector<orthia::NameInfo> names;
    DefaultUpdateVisibleItems(this, &m_namesOwner, m_namesBox, names,
        [&](auto it, auto vit)
    {

        vit->text.clear();
        vit->text.push_back(L"");

        vit->openHandler = []() {
        };
        vit->colorsHandler = nullptr;
    });
}
void CModulesWindow::SwitchActiveItem(int uid)
{
    m_model->SetActiveItem(uid);
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
void CModulesWindow::ConstructChilds()
{
    AddChild(m_modulesScrollable);
    AddChild(m_verticalScroll);
    AddChild(m_namesScrollable);
}
void CModulesWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    oui::Rect scrollRect = clientRect;
    scrollRect.size.width /= 2;
    m_modulesScrollable->Resize(scrollRect.size);

    m_verticalScroll->MoveTo(oui::Point(scrollRect.size.width, 0));
    m_verticalScroll->Resize(clientRect.size);


    oui::Rect namesScrollRect;
    namesScrollRect.position.x = scrollRect.size.width + 1;
    namesScrollRect.position.y = 0;
    namesScrollRect.size.width = clientRect.size.width - namesScrollRect.position.x;
    namesScrollRect.size.height = clientRect.size.height;
    m_namesScrollable->MoveTo(namesScrollRect.position);
    m_namesScrollable->Resize(namesScrollRect.size);

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
    oui::CPanelWindow::Invalidate(valid);
}