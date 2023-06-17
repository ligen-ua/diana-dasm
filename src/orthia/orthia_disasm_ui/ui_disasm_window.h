#pragma once
#include "oui_containers.h"
#include "orthia_model.h"
#include "oui_multiline_view.h"

class CDisasmWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner
{
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
    void ScrollUp(oui::MultiLineViewItem* item, int count) override;
    void ScrollDown(oui::MultiLineViewItem* item, int count) override;
    void ReloadVisibleData();
    void ConstructChilds() override;

    void OnResize() override;
    void SetFocusImpl() override;
public:
    CDisasmWindow(std::function<oui::String()> getCaption,
        std::shared_ptr<orthia::CProgramModel> model);
    void SetActiveItem(int itemUid);
};
