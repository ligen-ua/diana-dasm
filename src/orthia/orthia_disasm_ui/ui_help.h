#pragma once
#include "oui_modal.h"
#include "oui_editbox.h"
#include "oui_multiline_view.h"

namespace oui
{

    class CHelpWindow :public CMessageBoxWindow, oui::CSelfHostedMultiLineViewOwner
    {
        using Parent_type = CMessageBoxWindow;

    protected:
        std::shared_ptr<CMultiLineView> m_helpText;

        std::shared_ptr<oui::CMultiLineView> SF_GetView() override;
        oui::CConsole* SF_GetConsole() override;
    public:
        CHelpWindow();
        void ConstructChilds() override;
        void OnResize() override;
        bool Resize(const Size& newSize) override;
    };

    class CAboutBoxWindow :public CMessageBoxWindow
    {
        using Parent_type = CMessageBoxWindow;

    protected:
        std::shared_ptr<CEditBox> m_urlEdit;

        void OnPreDock(Rect& rect) override;

    public:
        CAboutBoxWindow();
        void ConstructChilds() override;
        void OnResize() override;
        bool Resize(const Size& newSize) override;
    };
}
