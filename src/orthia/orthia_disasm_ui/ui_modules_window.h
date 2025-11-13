#pragma once

#include "oui_containers.h"
#include "oui_listbox.h"
#include "ui_common.h"
#include "oui_scrollable.h"
#include "oui_label.h"

class CModulesWindow:public oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>, public IUIStatefulWindow
{
    using ParentType = oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>;

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

    std::shared_ptr<oui::CLabel> m_namesModuleLabel;
    orthia::Address_type m_selectedModuleAddress = 0;
    oui::String m_selectedModuleName;

    std::vector<orthia::NameInfo> m_cachedNamesPage;
    static const int g_nameCacheSize = 100;
    int m_lastTotalNamesCount = 0;
    int m_modulesWidthPercent = 42;

    std::function<void(orthia::Address_type address)> m_onGotoAddress;
    void SelectModule(orthia::Address_type address, const oui::String& name);

    oui::Rect CalcModulesScrollRect(const oui::Rect& clientRect);
    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    int Modules_GetTotalCount() const;
    void Modules_ShiftViewWindow(int newOffset);

    int Names_GetTotalCount() const;
    void Names_ShiftViewWindow(int newOffset);

    void UpdateVisibleItems();

    oui::String GetCurrentSelectedModule() const;
    void GotoAddress(orthia::Address_type address);
    void VertScroll_OnStartDrag(const oui::Point& point);

public:
    CModulesWindow(std::function<oui::String()> getCaption, 
        std::shared_ptr<orthia::CProgramModel> model,
        std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher,
        std::function<void (orthia::Address_type address)> onGotoAddress);
    void OnWorkspaceItemChanged();
    void SetActiveWorkspaceItem(int itemId) override;
    void Invalidate(bool valid = false) override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
};