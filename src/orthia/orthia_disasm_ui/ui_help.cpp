#include "ui_help.h"
#include "orthia_version.h"
#include "oui_menu.h"
#include "orthia_model_interfaces.h"

namespace oui
{
    static void PrepareText(std::vector<MultiLineViewItem>& lines)
    {
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("Global Hotkeys:");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  F10 or ALT       - Toggle Menu");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  ALT+Letter       - Enter Menu");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  Tab              - Focus next panel group");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  CTRL+Tab         - Focus next panel");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  ESC              - Close modal window");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  CTRL+Left/Right  - Scroll ListView");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("Disasm View Hotkeys:");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  G                - Goto Address");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  H                - Open History");
            lines.push_back(item);
        } 
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  ;                - Write a Comment");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  BACKSPACE        - Return to the previous location");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  CTRL+LMouseClick - Goto Address");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  CTRL+X           - Open XRef Dialog");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("Supported Commands:");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  x <mask>                - Examine symbols");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  u <address> [L<length>] - Unassemble address");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  lm                      - List loaded modules");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  d[b,w,d,q,p,ps]         - Display memory");
            lines.push_back(item);
        } 
        {

            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  threads                 - Display threads");
            lines.push_back(item);
        }
        {

            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  cls                     - Clear screen");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  .reload [<module>]      - Reload symbols for module");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  .analyze <module>       - Analyze module");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR(" -  .symfix [<path>]        - Set symbols directory");
            lines.push_back(item);
        }
        {
            MultiLineViewItem item;
            item.text = OUI_TCSTR("Note: pass CTRL+C to command edit box to stop the command");
            lines.push_back(item);
        }    }
    CHelpWindow::CHelpWindow()
        :
        Parent_type(
            [=]() {
        return OUI_TCSTR("Orthia Disassembler Help");
    },
            [=]() {
    })
    {
        IMultiLineViewOwner* owner = this;
        m_helpText = std::make_shared<CMultiLineView>(m_colorProfile, owner, false);
        std::vector<MultiLineViewItem> lines;
        PrepareText(lines);
        m_helpText->Init(std::move(lines));

        auto labelProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*labelProfile);
        labelProfile->normal.text = oui::ColorBrightYellow();
        labelProfile->mouseHighlight.text = oui::ColorBrightYellow();
        m_textLabel->SetColorProfile(labelProfile);
    }
    void CHelpWindow::OnContextMenu(const oui::Point& point)
    {
        if (!m_helpText->SelectionIsActive())
            return;

        auto contextMenuNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.panels.help.contextmenu"));
        std::vector<oui::PopupItem> items;
        items.push_back({ contextMenuNode->QueryValue(ORTHIA_TCSTR("copy")),
            [this]() { m_helpText->CopySelected(); } });

        oui::Point pointToUse{ point.x + 1, point.y + 1 };
        auto parent = GetPool()->GetRootWindow();
        auto popup = parent->AddChild_t(std::make_shared<oui::CMenuPopup>(std::move(items)));
        popup->Init(parent->GetPtr());
        popup->Dock(pointToUse);
        popup->SetFocus();
    }

    void CHelpWindow::ConstructChilds()
    {
        AddChild(m_helpText);
        Parent_type::ConstructChilds();
    }
    void CHelpWindow::OnResize()
    {
        Parent_type::OnResize();
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 3)
        {
            Size zeroSize;
            m_helpText->Resize(zeroSize);
            return;
        }

        Rect urlEditRect = clientRect;
        urlEditRect.position.x += 2;
        urlEditRect.position.y += 3;
        urlEditRect.size.width -= 4;
        urlEditRect.size.height -= 4;

        m_helpText->MoveTo(urlEditRect.position);
        m_helpText->Resize(urlEditRect.size);
    }
    bool CHelpWindow::Resize(const Size& newSize)
    {
        auto size = newSize;
        return Parent_type::Parent_type::Resize(size);
    }

    std::shared_ptr<oui::CMultiLineView> CHelpWindow::SF_GetView()
    {
        return m_helpText;
    }
    oui::CConsole* CHelpWindow::SF_GetConsole()
    {
        return GetConsole();
    }
    void CHelpWindow::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_helpText->SetFocus();
    }

    // CAboutBoxWindow
#ifdef _DEBUG
#define RELEASE_DEBUG_MODE "(Debug)"
#else
#define RELEASE_DEBUG_MODE "(Release)"
#endif
    CAboutBoxWindow::CAboutBoxWindow()
        : 
            Parent_type(
                                [=]() {
                            return OUI_TCSTR("Orthia Disassembler v" ORTHIA_UI_VERSION " " RELEASE_DEBUG_MODE);
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
    void CAboutBoxWindow::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_urlEdit->SetFocus();
    }
}