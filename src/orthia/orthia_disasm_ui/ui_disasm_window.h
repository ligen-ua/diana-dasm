#pragma once
#include "oui_containers.h"
#include "orthia_model.h"
#include "oui_multiline_view.h"


// Structure
// [PE HEADER]
// [SECTION HEADER]
// [FUNCTION HEADER]
// [INSTRUCTION HEADER]
class CDisasmWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner
{
    using Parent_type = oui::SimpleBrush<oui::CPanelWindow>;

    std::shared_ptr<orthia::CProgramModel> m_model;
    long long m_offset = 0;
    int m_itemUid = -1;

    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;


    void CancelAllQueries() override;
    void ScrollUp(oui::MultiLineViewItem* item, int count) override;
    void ScrollDown(oui::MultiLineViewItem* item, int count) override;
public:
    CDisasmWindow(std::function<oui::String()> getCaption,
        std::shared_ptr<orthia::CProgramModel> model);
    void SetActiveItem(int itemUid);
    void DoPaint(const oui::Rect& rect, oui::DrawParameters& parameters) override;
};
