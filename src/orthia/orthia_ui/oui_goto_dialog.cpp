#include "oui_goto_dialog.h"

namespace oui
{
    void CGotoDialog::ConstructChilds()
    {
        AddChild(m_okButton);
        AddChild(m_cancelButton);
    }

    CGotoDialog::CGotoDialog(const CommonDialogStrings& dialogStrings, RecipientHandler_type handler)
        :
        m_handler(handler)
    {
        auto buttonProfile = std::shared_ptr<ButtonColorProfile>(m_colorProfile, &m_colorProfile->button);

        SetCaption(dialogStrings.caption);
        m_okButton = std::make_shared<CButton>(buttonProfile, [text = dialogStrings.okText]() { return text;  });
        m_cancelButton = std::make_shared<CButton>(buttonProfile, [text = dialogStrings.cancelText]() { return text;  });


        this->RegisterSwitch(m_okButton);
        this->RegisterSwitch(m_cancelButton);
    }
    CGotoDialog::~CGotoDialog()
    {
    }

    void CGotoDialog::OnResize()
    {
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 5)
        {
            Size zeroSize;
            m_okButton->Resize(zeroSize);
            m_cancelButton->Resize(zeroSize);
            return;
        }
        int buttonWidth = clientRect.size.width / 3;

        int okButtonX = clientRect.position.x + 2;
        int cancelButtonX = clientRect.position.x + clientRect.size.width - 2 - buttonWidth;

        {
            Rect boxRect;
            boxRect.position.x = okButtonX;
            boxRect.position.y = clientRect.size.height - 2;
            boxRect.size.width = buttonWidth;
            boxRect.size.height = 1;
            m_okButton->MoveTo(boxRect.position);
            m_okButton->Resize(boxRect.size);
        }

        if (cancelButtonX > 0)
        {
            Rect boxRect;
            boxRect.position.x = cancelButtonX;
            boxRect.position.y = clientRect.size.height - 2;
            boxRect.size.width = buttonWidth;
            boxRect.size.height = 1;
            m_cancelButton->MoveTo(boxRect.position);
            m_cancelButton->Resize(boxRect.size);
        }
        else
        {
            Size zeroSize;
            m_cancelButton->Resize(zeroSize);
        }

        Parent_type::OnResize();
    }

}