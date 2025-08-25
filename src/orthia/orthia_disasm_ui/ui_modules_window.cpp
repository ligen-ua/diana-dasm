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


    auto columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.modules.columns"));

    oui::IListBoxOwner* owner = this;
    m_itemsBox = std::make_shared<oui::CListBox>(m_colorProfile, owner);
    m_itemsBox->InitColumns(oui::ColumnParam([=] { return columnsNode->QueryValue(L"name");  }),
        oui::ColumnParam([=] { return columnsNode->QueryValue(L"address");  })
    );
    m_itemsBox->SetBorderStyle(oui::BorderStyle::None);
}
int CModulesWindow::GetTotalCount() const
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        return activeItem->GetModulesCount();
    }
    return 0;
}
void CModulesWindow::CancelAllQueries()
{

}
void CModulesWindow::UpdateVisibleItems()
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        activeItem->GetModules(items);
    }
    
    DefaultUpdateVisibleItems(this, this, m_itemsBox, items,
        [&](auto it, auto vit)
    {
        std::wstring name;
        orthia::UnparseFileNameFromFullFileName(it->fullName, &name);

        vit->text.clear();
        vit->text.push_back(name);
        vit->text.push_back(orthia::ToWideStringAsHex(it->address));

        vit->openHandler = []() {
        };
        vit->colorsHandler = nullptr;
    });
}
void CModulesWindow::SwitchActiveItem(int uid)
{
    m_model->SetActiveItem(uid);
}

void CModulesWindow::ShiftViewWindow(int newOffset)
{
    auto activeItem = m_model->GetActiveItem();
    std::vector<orthia::ModuleInfo> items;
    if (activeItem)
    {
        activeItem->GetModules(items);
    }
 
    DefaultShiftViewWindow(m_itemsBox, newOffset, items.size());
    UpdateVisibleItems();

}
void CModulesWindow::OnVisibleItemChanged()
{

}
bool CModulesWindow::ShiftViewWindowToSymbol(const oui::String& symbol)
{
    return false;
}
void CModulesWindow::ConstructChilds()
{
    AddChild(m_itemsBox);
}
void CModulesWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    m_itemsBox->Resize(clientRect.size);
    UpdateVisibleItems();
}
void CModulesWindow::SetFocusImpl()
{
    UpdateVisibleItems();
    m_itemsBox->SetFocus();
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