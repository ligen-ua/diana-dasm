#pragma once
#include "oui_containers.h"
#include "oui_multiline_view.h"
#include "ui_common.h"

class CDisasmWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner, public IUIStatefulWindow
{
    static const int field_peAddress = 1;

    using Parent_type = oui::SimpleBrush<oui::CPanelWindow>;

    std::shared_ptr<orthia::CProgramModel> m_model;

    // these vars form a content iterator
    int m_metaInfoPos = 0;
    DI_UINT64 m_peAddress = 0;
    DI_UINT64 m_peAddressEnd = 0;
    bool m_userSuppliedPeAddress = false;

    int m_itemUid = -1;

    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void CancelAllQueries() override;
    bool ScrollUp(oui::MultiLineViewItem* item, int count) override;
    bool ScrollDown(oui::MultiLineViewItem* item, int count) override;
    void ReloadVisibleData();

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;
    void SetActiveItemImpl(int itemUid);

public:
    CDisasmWindow(std::function<oui::String()> getCaption,
        std::shared_ptr<orthia::CProgramModel> model);
    void SetActiveItem(int itemUid, DI_UINT64 initialAddressHint = 0);

    // IUIStatefulWindow
    void ReloadState(const UIState& state) override;
    void SaveState(UIState& state) override;
    void SetActiveWorkspaceItem(int itemId) override;
};
