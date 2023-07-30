#define _CRT_NON_CONFORMING_SWPRINTFS
#define _CRT_SECURE_NO_WARNINGS

#include "ui_workspace_window.h"
#include <ctime>

CWorkspaceWindow::CWorkspaceWindow(std::function<oui::String()> getCaption, std::shared_ptr<orthia::CProgramModel> model)
    : 
     oui::SimpleBrush<oui::CPanelWindow>(getCaption),
     m_model(model)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());

    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    oui::IListBoxOwner* owner = this;
    m_itemsBox = std::make_shared<oui::CListBox>(m_colorProfile, owner);
    m_itemsBox->InitColumns(1);
    m_itemsBox->SetBorderStyle(oui::BorderStyle::None);
}
int CWorkspaceWindow::GetTotalCount() const
{
    return 0;
}
void CWorkspaceWindow::CancelAllQueries()
{

}
void CWorkspaceWindow::UpdateVisibleItems()
{
    std::vector<orthia::WorkplaceItem> items;
    auto activeIndex = m_model->QueryWorkspaceItems(items);
    auto selectedIt = items.end();
    if (activeIndex != -1)
    {
        selectedIt = items.begin() + activeIndex;
    }
    DefaultUpdateVisibleItems(this, this, m_itemsBox, items,
        [&](auto it, auto vit)
    {
        auto name = OUI_TO_STR(1 + (it - items.begin())) + OUI_STR(". ") + it->name.native;
        if (selectedIt == it)
        {
            name = OUI_STR("> ") + name;
        }
        vit->text.clear();
        vit->text.push_back(name);  

        vit->openHandler = [=, uid = it->uid]() {
            SwitchActiveItem(uid);
        };
        vit->colorsHandler = nullptr;
    });
}
void CWorkspaceWindow::SwitchActiveItem(int uid)
{
    m_model->SetActiveItem(uid);
}
void CWorkspaceWindow::ShiftViewWindow(int newOffset)
{
    std::vector<orthia::WorkplaceItem> items;
    m_model->QueryWorkspaceItems(items);
 
    DefaultShiftViewWindow(m_itemsBox, newOffset, items.size());
    UpdateVisibleItems();

}
void CWorkspaceWindow::OnVisibleItemChanged()
{

}
bool CWorkspaceWindow::ShiftViewWindowToSymbol(const oui::String& symbol)
{
    return false;
}
void CWorkspaceWindow::ConstructChilds()
{
    AddChild(m_itemsBox);
}
void CWorkspaceWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    m_itemsBox->Resize(clientRect.size);
}
void CWorkspaceWindow::SetFocusImpl()
{
    m_itemsBox->SetFocus();
}
void CWorkspaceWindow::OnWorkspaceItemChanged()
{
    UpdateVisibleItems();
}
void CWorkspaceWindow::SetActiveWorkspaceItem(int itemId)
{
    OnWorkspaceItemChanged();
}
void CWorkspaceWindow::Invalidate(bool valid)
{
    oui::CPanelWindow::Invalidate(valid);
}