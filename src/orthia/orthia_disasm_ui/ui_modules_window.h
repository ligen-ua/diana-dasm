#pragma once

#include "oui_containers.h"
#include "oui_listbox.h"
#include "ui_common.h"
#include "oui_scrollable.h"

class CModulesWindow:public oui::SimpleBrush<oui::CPanelWindow>, public IUIStatefulWindow
{
    std::shared_ptr<orthia::CProgramModel> m_model;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    // left part
    oui::ListBoxOwnerProxy m_modulesOwner;
    std::shared_ptr<oui::CListBox> m_modulesBox;
    std::shared_ptr<oui::CScrollable> m_modulesScrollable;
    
    // optional part
    std::shared_ptr<oui::CVerticalScrollBar> m_verticalScroll;

    // names part
    oui::ListBoxOwnerProxy m_namesOwner;
    std::shared_ptr<oui::CListBox> m_namesBox;
    std::shared_ptr<oui::CScrollable> m_namesScrollable;

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    int Modules_GetTotalCount() const;
    void Modules_ShiftViewWindow(int newOffset);

    void SwitchActiveItem(int uid);
    void UpdateVisibleItems();
public:
    CModulesWindow(std::function<oui::String()> getCaption, std::shared_ptr<orthia::CProgramModel> model);
    void OnWorkspaceItemChanged();
    void SetActiveWorkspaceItem(int itemId) override;
    void Invalidate(bool valid = false) override;
};