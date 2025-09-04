#pragma once

#include "oui_modal.h"
#include "oui_editbox.h"
#include "oui_button.h"

namespace oui
{
    class CGotoDialog:public oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>
    {
        using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>;

    public:
        using  RecipientHandler_type = std::function<bool(std::shared_ptr<oui::CGotoDialog> dlg, const String& text)>;

    private:
        std::shared_ptr<CButton> m_okButton;
        std::shared_ptr<CButton> m_cancelButton;
        RecipientHandler_type m_handler;

    protected:
        void OnResize() override;
        void ConstructChilds() override;

    public:
        CGotoDialog(const CommonDialogStrings& dialogStrings, RecipientHandler_type handler);
        ~CGotoDialog();
    };

}
