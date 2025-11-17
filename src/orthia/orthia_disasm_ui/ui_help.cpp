#include "ui_help.h"


namespace oui
{

    CAboutBoxWindow::CAboutBoxWindow()
        : 
            Parent_type(
                                [=]() {
                            return OUI_TCSTR("Orthia Disassembler");
                        },
                                [=]() {
                        })
    {
        m_urlEdit = std::make_shared<CEditBox>(m_colorProfile);
        m_urlEdit->SetReadOnly(true);
        m_urlEdit->SetText(OUI_TCSTR("https://github.com/ligen-ua/diana-dasm"));

        auto labelProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*labelProfile);
        labelProfile->normal.text = oui::ColorBrightYellow();
        labelProfile->mouseHighlight.text = oui::ColorBrightYellow();
        m_textLabel->SetColorProfile(labelProfile);
    }
    void CAboutBoxWindow::ConstructChilds() 
    {
        AddChild(m_urlEdit);
        Parent_type::ConstructChilds();
    }
    void CAboutBoxWindow::OnResize() 
    {
        Parent_type::OnResize();
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 3)
        {
            Size zeroSize;
            m_urlEdit->Resize(zeroSize);
            return;
        }

        Rect urlEditRect = clientRect;
        urlEditRect.position.x += 2;
        urlEditRect.position.y += 3;
        urlEditRect.size.width -= 4;
        urlEditRect.size.height = 1;

        m_urlEdit->MoveTo(urlEditRect.position);
        m_urlEdit->Resize(urlEditRect.size);
    }
    bool CAboutBoxWindow::Resize(const Size& newSize)
    {
        auto size = newSize;
        size.height = 7;
        return Parent_type::Parent_type::Resize(size);
    }
    void CAboutBoxWindow::OnPreDock(Rect& rect)
    {
        const int maxWidth = 50;
        if (rect.size.width < maxWidth)
        {
            return;
        }
        int diff = rect.size.width - maxWidth;
        rect.position.x += diff / 2;
        rect.size.width = maxWidth;
    }
}