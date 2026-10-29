#pragma once

#include "oui_modal.h"
#include "oui_label.h"
#include "oui_button.h"
#include "oui_checkbox.h"
#include "orthia_config.h"

namespace oui
{
    // After a target opened with a new database: what to start, the code analysis and the
    // symbols. OK starts what is checked; Cancel, Esc and [x] start nothing.
    // The handler is called once, either way
    class CAutoTasksDialog : public SimpleBrush<CModalWindow>
    {
        using Parent_type = SimpleBrush<CModalWindow>;

        std::shared_ptr<CLabel>    m_messageLabel;
        std::shared_ptr<CCheckBox> m_analyzeCheck;
        std::shared_ptr<CCheckBox> m_symbolsCheck;
        std::shared_ptr<CLabel>    m_hintLabel;
        std::shared_ptr<CButton>   m_okButton;
        std::shared_ptr<CButton>   m_cancelButton;

        std::function<void(const orthia::AutoTasks&)> m_handler;
        bool m_handled = false;

        void Confirm();
        void Reply(const orthia::AutoTasks& tasks);

    protected:
        void ConstructChilds() override;
        void OnResize() override;
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
        void OnPreDock(Rect& rect) override;
        void OnFinishDialog() override;

    public:
        struct Strings
        {
            String message;     // "No database for ... yet. Start now:"
            String analyze;
            String symbols;
            String hint;
            String ok;
            String cancel;
        };
        CAutoTasksDialog(const Strings& strings,
            const orthia::AutoTasks& initial,
            std::function<void(const orthia::AutoTasks&)> handler);

        bool Resize(const Size& newSize) override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };
}
