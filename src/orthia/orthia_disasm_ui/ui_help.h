#pragma once
#include "oui_modal.h"
#include "oui_editbox.h"

namespace oui
{

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
