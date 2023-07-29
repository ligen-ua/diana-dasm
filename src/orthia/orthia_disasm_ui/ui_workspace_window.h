#pragma once

#include "oui_containers.h"
#include "oui_listbox.h"
#include "ui_common.h"

class CWorkspaceWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IListBoxOwner, public IUIStatefulWindow
{
    std::shared_ptr<orthia::CProgramModel> m_model;
    std::shared_ptr<oui::CListBox> m_itemsBox;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    int GetTotalCount() const override;
    void CancelAllQueries() override;
    void ShiftViewWindow(int newOffset) override;
    void OnVisibleItemChanged() override;
    bool ShiftViewWindowToSymbol(const oui::String & symbol) override;

    void SwitchActiveItem(int uid);
    void UpdateVisibleItems();
public:
    CWorkspaceWindow(std::function<oui::String()> getCaption, std::shared_ptr<orthia::CProgramModel> model);
    void OnWorkspaceItemChanged();
    void SetActiveWorkspaceItem(int itemId) override;
};