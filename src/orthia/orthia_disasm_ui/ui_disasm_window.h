#pragma once
#include "oui_containers.h"
#include "orthia_model.h"

struct DisasmLine
{
    oui::String text;
};
class CDisasmWindow:public oui::SimpleBrush<oui::CPanelWindow>
{
    using Parent_type = oui::SimpleBrush<oui::CPanelWindow>;

    std::shared_ptr<orthia::CProgramModel> m_model;
    long long m_offset = 0;
    int m_itemUid = -1;

    std::vector<DisasmLine> m_lines;
    int m_selectedLine = 0;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    static oui::String m_chunk;
public:
    CDisasmWindow(std::function<oui::String()> getCaption,
        std::shared_ptr<orthia::CProgramModel> model);
    void SetActiveItem(int itemUid);
    void DoPaint(const oui::Rect& rect, oui::DrawParameters& parameters) override;
};
