#include "ui_disasm_window.h"

oui::String CDisasmWindow::m_chunk;

CDisasmWindow::CDisasmWindow(std::function<oui::String()> getCaption,
    std::shared_ptr<orthia::CProgramModel> model)
    :
        m_model(model),
        oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);
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

    auto console = GetConsole();
    if (!console)
    {
        return;
    }
    const auto colorProfile = m_colorProfile->listBox;
    const auto absClientRect = GetAbsoluteClientRect(this, rect);

    auto currentItem = m_lines.begin();
    for (int i = 0; i < absClientRect.size.height; ++i)
    {
        if (currentItem == m_lines.end())
        {
            break;
        }

        auto color = &colorProfile.normalText;
        if (i == m_selectedLine)
        {
            color = &colorProfile.selectedText;
        }
        const int yPos = i + absClientRect.position.y;
        const int leftX = 2;
        m_chunk = currentItem->text;
        
        oui::Point pt{ leftX, yPos };
        parameters.console.PaintText(pt,
            color->text,
            color->background,
            m_chunk.native);

        ++currentItem;
    }
}

