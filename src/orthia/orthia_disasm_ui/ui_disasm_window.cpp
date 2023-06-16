#include "ui_disasm_window.h"

// == Structure ==
// [PE HEADER]
// [SECTION HEADER]
// [FUNCTION HEADER]
// [INSTRUCTION HEADER]

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
    m_peAddress = 0;
    m_metaInfoPos = 0;

    ReloadVisibleData();
    Invalidate();
}
void CDisasmWindow::ReloadVisibleData()
{
    
    std::vector<oui::MultiLineViewItem> items;

    m_view->Init(std::move(items));
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
void CDisasmWindow::OnResize()
{
    int prevHeight = m_view->GetSize().height;
    const oui::Rect clientRect = GetClientRect();
    
    m_view->Resize(clientRect.size);

    if (clientRect.size.height > prevHeight)
    {
        ReloadVisibleData();
    }
}
void CDisasmWindow::SetFocusImpl()
{
    m_view->SetFocus();
}



