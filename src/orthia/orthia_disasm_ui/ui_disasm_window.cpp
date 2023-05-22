#include "ui_disasm_window.h"

CDisasmWindow::CDisasmWindow(std::function<oui::String()> getCaption,
    std::shared_ptr<orthia::CProgramModel> model)
    :
    m_model(model),
    oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
}
void CDisasmWindow::SetActiveItem(int itemUid)
{
    m_itemUid = itemUid;
    m_offset = 0;
    Invalidate();
}
void CDisasmWindow::DoPaint(const oui::Rect& rect, oui::DrawParameters& parameters)
{
    Parent_type::DoPaint(rect, parameters);
    if (m_itemUid)
    {
        // m_model->Get
    }
}

