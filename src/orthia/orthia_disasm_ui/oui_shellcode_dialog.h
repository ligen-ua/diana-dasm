#pragma once

#include "oui_modal.h"
#include "oui_label.h"
#include "oui_editbox.h"
#include "oui_button.h"
extern "C"
{
#include "diana_core.h"
}

namespace oui
{
    class CShellcodeDialog : public SimpleBrush<CModalWindow>
    {
        using Parent_type = SimpleBrush<CModalWindow>;

        std::shared_ptr<CLabel>   m_messageLabel;
        std::shared_ptr<CLabel>   m_addressLabel;
        std::shared_ptr<CEditBox> m_addressEdit;
        std::shared_ptr<CLabel>   m_modeLabel;
        std::shared_ptr<CButton>  m_archButton;     // "< x64 >": Left/Right, Enter or a click change it
        std::shared_ptr<CButton>  m_okButton;
        std::shared_ptr<CButton>  m_cancelButton;

        std::function<String()> m_getMessageText;
        std::function<String()> m_getAddressLabelText;
        std::function<String()> m_getModeLabelText;
        std::function<void(DI_UINT64 address, int dianaMode, int dianaArch)> m_handler;
        std::function<void()> m_cancelHandler;
        bool m_confirmed = false;
        size_t m_archIndex = 0;                     // in orthia::GetDianaArchChoices()

        void TryConfirm();
        void ShiftArch(int delta);

    protected:
        void ConstructChilds() override;
        void OnResize() override;
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
        void OnPreDock(Rect& rect) override;
        void OnFinishDialog() override;

    public:
        CShellcodeDialog(
            std::function<String()> getMessageText,
            std::function<String()> getAddressLabelText,
            std::function<String()> getModeLabelText,
            std::function<void(DI_UINT64 address, int dianaMode, int dianaArch)> handler);

        void SetCancelHandler(std::function<void()> handler);
        bool Resize(const Size& newSize) override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };
}
