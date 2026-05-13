#pragma once

#include "oui_containers.h"
#include "oui_listbox.h"
#include "oui_editbox.h"
#include "oui_label.h"
#include "oui_scrollable.h"
#include "ui_common.h"

class CSectionsWindow : public oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>,
                        public IUIStatefulWindow
{
    using ParentType = oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>;

    std::shared_ptr<orthia::CProgramModel> m_model;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    // top bar
    std::shared_ptr<oui::CLabel>   m_headerLabel;
    std::shared_ptr<oui::CEditBox> m_addressEdit;

    // left pane: sections list
    std::vector<orthia::SectionInfo> m_cachedSections;
    oui::ListBoxOwnerProxy           m_sectionsOwner;
    std::shared_ptr<oui::CListBox>   m_sectionsBox;
    std::shared_ptr<oui::CScrollable> m_sectionsScrollable;

    // splitter
    std::shared_ptr<oui::CVerticalScrollBar> m_verticalScroll;
    int m_sectionsWidthPercent = 45;

    // right pane: attributes of selected section
    const orthia::SectionInfo*        m_selectedSection = nullptr;
    oui::ListBoxOwnerProxy            m_attrsOwner;
    std::shared_ptr<oui::CListBox>    m_attrsBox;
    std::shared_ptr<oui::CScrollable> m_attrsScrollable;

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    oui::Rect CalcSectionsRect(const oui::Rect& clientRect) const;

    int Sections_GetTotalCount() const;
    void Sections_ShiftViewWindow(int newOffset);
    void Sections_OnVisibleItemChanged();

    int Attrs_GetTotalCount() const;
    void Attrs_ShiftViewWindow(int newOffset);

    void UpdateSections();
    void UpdateSectionsVisibleItems();
    void UpdateAttrsVisibleItems();

    void VertScroll_OnStartDrag(const oui::Point& point);

    static const int field_moduleAddress        = 1;
    static const int field_sectionsBox_Offset   = 2;
    static const int field_sectionsBox_Position = 3;
    static const int field_attrsBox_Offset      = 4;

public:
    CSectionsWindow(std::function<oui::String()> getCaption,
                    std::shared_ptr<orthia::CProgramModel> model,
                    std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher);

    void NavigateTo(orthia::Address_type moduleBase, const oui::String& displayName);

    void SetActiveWorkspaceItem(int itemId) override;
    void Invalidate(bool valid = false) override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
    void ReloadState(const UIState& state) override;
    void SaveState(UIState& state) override;
};
