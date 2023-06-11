#include "ui_output_window.h"

COutputWindow::COutputWindow(std::function<oui::String()> getCaption)
    : 
     oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    // FOR WIN LOGIC DEBUG
    //    SetBackgroundColor(oui::ColorBlue());

    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    oui::IMultiLineViewOwner* param = this;
    m_view = std::make_shared<oui::CMultiLineView>(m_colorProfile, param);

}
void COutputWindow::AddLine(const oui::String& line)
{
    oui::MultiLineViewItem item;
    item.text = line;
    m_view->AddLine(std::move(item));
}
void COutputWindow::CancelAllQueries()
{
}
void COutputWindow::ScrollUp(oui::MultiLineViewItem* item, int count)
{
}
void COutputWindow::ScrollDown(oui::MultiLineViewItem* item, int count)
{
}
void COutputWindow::ConstructChilds()
{
    AddChild(m_view);
}
void COutputWindow::OnResize()
{
    const oui::Rect clientRect = GetClientRect();
    m_view->Resize(clientRect.size);
}
void COutputWindow::SetFocusImpl()
{
    m_view->SetFocus();
}