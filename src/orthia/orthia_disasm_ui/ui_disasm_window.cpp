#include "ui_disasm_window.h"

CDisasmWindow::CDisasmWindow(std::function<oui::String()> getCaption,
    std::shared_ptr<orthia::CProgramModel> model)
    :
        m_model(model),
        oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    oui::IMultiLineViewOwner* param = this;
    m_view = std::make_shared<oui::CMultiLineView>(m_colorProfile, param);

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

}
void CDisasmWindow::CancelAllQueries()
{
}
void CDisasmWindow::ScrollUp(oui::MultiLineViewItem* item, int count) 
{
}
void CDisasmWindow::ScrollDown(oui::MultiLineViewItem* item, int count) 
{
}


