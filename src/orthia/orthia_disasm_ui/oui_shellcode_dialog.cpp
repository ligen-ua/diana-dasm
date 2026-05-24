#include "oui_shellcode_dialog.h"
#include "oui_color.h"

namespace oui
{
    static bool ParseHexAddress(const String& text, DI_UINT64& out)
    {
        auto str = text.native;
        if (str.empty())
            return false;
        // strip optional "0x" / "0X" prefix
        if (str.size() >= 2 &&
            str[0] == String::char_type('0') &&
            (str[1] == String::char_type('x') || str[1] == String::char_type('X')))
        {
            str = str.substr(2);
        }
        if (str.empty())
            return false;
        try {
            size_t pos = 0;
            out = (DI_UINT64)std::stoull(str, &pos, 16);
            return pos == str.size();
        }
        catch (...) {
            return false;
        }
    }

    static bool ParseMode(const String& text, int& dianaMode)
    {
        if (text.native == OUI_TCSTR("32")) { dianaMode = DIANA_MODE32; return true; }
        if (text.native == OUI_TCSTR("64")) { dianaMode = DIANA_MODE64; return true; }
        return false;
    }

    CShellcodeDialog::CShellcodeDialog(
        std::function<String()> getMessageText,
        std::function<String()> getAddressLabelText,
        std::function<String()> getModeLabelText,
        std::function<void(DI_UINT64, int)> handler)
        : m_getMessageText(std::move(getMessageText))
        , m_getAddressLabelText(std::move(getAddressLabelText))
        , m_getModeLabelText(std::move(getModeLabelText))
        , m_handler(std::move(handler))
    {
        auto labelProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*labelProfile);

        m_messageLabel = std::make_shared<CLabel>(labelProfile, [this]() { return m_getMessageText(); });

        auto captionProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*captionProfile);
        captionProfile->normal.text = ColorBrightYellow();
        captionProfile->mouseHighlight.text = ColorBrightYellow();

        m_addressLabel = std::make_shared<CLabel>(captionProfile, [this]() { return m_getAddressLabelText(); });
        m_addressEdit  = std::make_shared<CEditBox>(m_colorProfile);
        m_addressEdit->SetText(String(OUI_STR("0x400000")));
        m_addressEdit->SetSelectAllOnFocus(true);

        m_modeLabel = std::make_shared<CLabel>(captionProfile, [this]() { return m_getModeLabelText(); });
        m_modeEdit  = std::make_shared<CEditBox>(m_colorProfile);
        m_modeEdit->SetText(String(OUI_STR("64")));
        m_modeEdit->SetSelectAllOnFocus(true);

        m_addressEdit->SetEnterHandler([this](const String&) { m_modeEdit->SetFocus(); });
        m_modeEdit->SetEnterHandler([this](const String&) { TryConfirm(); });

        auto buttonProfile = std::make_shared<ButtonColorProfile>(m_colorProfile->button);
        m_okButton = std::make_shared<CButton>(buttonProfile, []() { return String(OUI_STR("OK")); });
        m_okButton->SetClickHandler([this]() { TryConfirm(); });

        m_cancelButton = std::make_shared<CButton>(buttonProfile, []() { return String(OUI_STR("Cancel")); });
        m_cancelButton->SetClickHandler([this]() { FinishDialog(); });
    }

    void CShellcodeDialog::TryConfirm()
    {
        DI_UINT64 address = 0;
        if (!ParseHexAddress(m_addressEdit->GetText(), address))
            return;

        int dianaMode = 0;
        if (!ParseMode(m_modeEdit->GetText(), dianaMode))
            return;

        m_handler(address, dianaMode);
        FinishDialog();
    }

    void CShellcodeDialog::ConstructChilds()
    {
        AddChild(m_messageLabel);
        AddChild(m_addressLabel);
        AddChild(m_addressEdit);
        AddChild(m_modeLabel);
        AddChild(m_modeEdit);
        AddChild(m_okButton);
        AddChild(m_cancelButton);
    }

    void CShellcodeDialog::OnResize()
    {
        const auto clientRect = GetClientRect();
        if (clientRect.size.width < 5 || clientRect.size.height < 10)
        {
            Size zero;
            m_messageLabel->Resize(zero);
            m_addressLabel->Resize(zero);
            m_addressEdit->Resize(zero);
            m_modeLabel->Resize(zero);
            m_modeEdit->Resize(zero);
            m_okButton->Resize(zero);
            m_cancelButton->Resize(zero);
            return;
        }

        const int x     = clientRect.position.x + 2;
        const int width = clientRect.size.width - 4;
        const int y0    = clientRect.position.y;

        auto placeRow = [&](std::shared_ptr<CWindow> wnd, int row, int w, int offsetX = 0)
        {
            Point p{ x + offsetX, y0 + row };
            wnd->MoveTo(p);
            wnd->Resize(Size{ w, 1 });
        };

        placeRow(m_messageLabel, 1, width);
        placeRow(m_addressLabel, 3, width);
        placeRow(m_addressEdit,  4, width);
        placeRow(m_modeLabel,    6, width);
        placeRow(m_modeEdit,     7, width);

        // buttons on row 9: OK on the left, Cancel on the right
        const int btnWidth = 12;
        placeRow(m_okButton,     9, btnWidth);
        placeRow(m_cancelButton, 9, btnWidth, width - btnWidth);

        Parent_type::OnResize();
    }

    bool CShellcodeDialog::Resize(const Size& newSize)
    {
        auto size = newSize;
        size.height = 13;
        return Parent_type::Resize(size);
    }

    void CShellcodeDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool>)
    {
        m_addressEdit->SetFocus();
    }

    void CShellcodeDialog::OnPreDock(Rect& rect)
    {
        const int minWidth = 54;
        if (rect.size.width < minWidth)
        {
            int diff = minWidth - rect.size.width;
            rect.position.x -= diff / 2;
            rect.size.width = minWidth;
        }
    }

    bool CShellcodeDialog::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.keyEvent.valid && evt.keyEvent.virtualKey == VirtualKey::Tab)
        {
            auto pool = GetPool();
            if (pool)
            {
                // cycle: addressEdit → modeEdit → okButton → cancelButton → addressEdit
                auto focused = pool->GetFocus();
                if      (focused == m_addressEdit)  m_modeEdit->SetFocus();
                else if (focused == m_modeEdit)     m_okButton->SetFocus();
                else if (focused == m_okButton)     m_cancelButton->SetFocus();
                else                                m_addressEdit->SetFocus();
            }
            return true;
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
}
