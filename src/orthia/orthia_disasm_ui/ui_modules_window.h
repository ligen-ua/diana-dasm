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
    std::vector<orthia::ModuleInfo> m_lastModules;
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

    int m_requiredNamesCacheSize = 0;
    int m_requiredNamesOffset = 0;
    int m_requiredModulesBoxOffset = 0;
    int m_requiredModulesBoxPosition = 0;
    bool m_needUpdateModulesBox = false;

    std::vector<orthia::NameInfo> m_cachedNamesPage;
    static const int g_nameCacheSize = 100;
    int m_lastTotalNamesCount = 0;
    int m_modulesWidthPercent = 42;

    static const int field_selectedModuleAddress = 1;
    static const int field_selectedModuleName = 2;
    static const int field_namesBox_Offset = 3;
    static const int field_cachedNamesPage_size = 4;
    static const int field_modulesBox_Offset = 5;
    static const int field_modulesBox_Position = 6;

    std::function<void(orthia::Address_type address)> m_onGotoAddress;
    void SelectModule(orthia::Address_type address, const oui::String& name);

    oui::Rect CalcModulesScrollRect(const oui::Rect& clientRect);
    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    int Modules_GetTotalCount() const;
    void Modules_ShiftViewWindow(int newOffset);
    bool Modules_ShiftViewWindowToSymbol(const oui::String& symbol);

    int Names_GetTotalCount() const;
    void Names_ShiftViewWindow(int newOffset);

    void UpdateVisibleItems();

    oui::String GetCurrentSelectedModule() const;
    void GotoAddress(orthia::Address_type address);
    void VertScroll_OnStartDrag(const oui::Point& point);

    template<class OwnerType, class ContainerType, class Predicate>
    friend bool oui::DefaultShiftViewWindowToSymbol(OwnerType* owner,
            std::shared_ptr<oui::CListBox> listBox,
            const oui::String& symbol,
            ContainerType& container,
            Predicate predicate);
public:
    CModulesWindow(std::function<oui::String()> getCaption, 
        std::shared_ptr<orthia::CProgramModel> model,
        std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher,
        std::function<void (orthia::Address_type address)> onGotoAddress);
    void OnWorkspaceItemChanged();
    void SetActiveWorkspaceItem(int itemId) override;
    void Invalidate(bool valid = false) override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
    void ReloadState(const UIState& state) override;
    void SaveState(UIState& state)override;

    void HighlightItem(int highlightItemOffset);

};