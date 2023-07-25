#pragma once

#include "oui_containers.h"
#include "oui_listbox.h"
#include "orthia_model.h"

class CWorkspaceWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IListBoxOwner
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

    void UpdateVisibleItems();
public:
    CWorkspaceWindow(std::function<oui::String()> getCaption, std::shared_ptr<orthia::CProgramModel> model);
    void OnWorkspaceItemChanged();
};