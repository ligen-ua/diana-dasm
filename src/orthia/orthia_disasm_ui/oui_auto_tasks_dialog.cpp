#include "oui_auto_tasks_dialog.h"
#include "oui_color.h"

namespace oui
{
    CAutoTasksDialog::CAutoTasksDialog(const Strings& strings,
        const orthia::AutoTasks& initial,
        std::function<void(const orthia::AutoTasks&)> handler)
        : m_handler(std::move(handler))
    {
        auto labelProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*labelProfile);
        auto hintProfile = std::make_shared<LabelColorProfile>();
        QueryDefaultColorProfile(*hintProfile);
        hintProfile->normal.text = ColorGray();
        hintProfile->mouseHighlight.text = ColorGray();

        auto buttonProfile = std::make_shared<ButtonColorProfile>(m_colorProfile->button);

        const String message = strings.message;
        const String analyze = strings.analyze;
        const String symbols = strings.symbols;
        const String hint = strings.hint;
        const String ok = strings.ok;
        const String cancel = strings.cancel;

        m_messageLabel = std::make_shared<CLabel>(labelProfile, [message]() { return message; });
        m_analyzeCheck = std::make_shared<CCheckBox>(buttonProfile, [analyze]() { return analyze; }, initial.analyze);
        m_symbolsCheck = std::make_shared<CCheckBox>(buttonProfile, [symbols]() { return symbols; }, initial.symbols);
        m_hintLabel = std::make_shared<CLabel>(hintProfile, [hint]() { return hint; });

        m_okButton = std::make_shared<CButton>(buttonProfile, [ok]() { return ok; });
        m_okButton->SetClickHandler([this]() { Confirm(); });

        m_cancelButton = std::make_shared<CButton>(buttonProfile, [cancel]() { return cancel; });
        m_cancelButton->SetClickHandler([this]() { FinishDialog(); });
    }

    void CAutoTasksDialog::Reply(const orthia::AutoTasks& tasks)
    {
        if (m_handled)
        {
            return;
        }
        m_handled = true;
        if (m_handler)
        {
            m_handler(tasks);
        }
    }

    void CAutoTasksDialog::Confirm()
    {
        orthia::AutoTasks tasks;
        tasks.analyze = m_analyzeCheck->IsChecked();
        tasks.symbols = m_symbolsCheck->IsChecked();
        Reply(tasks);
        FinishDialog();
    }

    void CAutoTasksDialog::OnFinishDialog()
    {
        // Cancel, Esc, [x]: the target stays open, nothing starts
        orthia::AutoTasks none;
        none.analyze = false;
        none.symbols = false;
        Reply(none);
        Parent_type::OnFinishDialog();
    }

    void CAutoTasksDialog::ConstructChilds()
    {
        AddChild(m_messageLabel);
        AddChild(m_analyzeCheck);
        AddChild(m_symbolsCheck);
        AddChild(m_hintLabel);
        AddChild(m_okButton);
        AddChild(m_cancelButton);
    }

    void CAutoTasksDialog::OnResize()
    {
        const auto clientRect = GetClientRect();
        if (clientRect.size.width < 5 || clientRect.size.height < 8)
        {
            Size zero;
            m_messageLabel->Resize(zero);
            m_analyzeCheck->Resize(zero);
            m_symbolsCheck->Resize(zero);
            m_hintLabel->Resize(zero);
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
        placeRow(m_analyzeCheck, 3, width - 2, 2);
        placeRow(m_symbolsCheck, 4, width - 2, 2);
        placeRow(m_hintLabel,    6, width);

        // buttons on row 8: OK on the left, Cancel on the right
        const int btnWidth = 12;
        placeRow(m_okButton,     8, btnWidth);
        placeRow(m_cancelButton, 8, btnWidth, width - btnWidth);

        Parent_type::OnResize();
    }

    bool CAutoTasksDialog::Resize(const Size& newSize)
    {
        auto size = newSize;
        size.height = 12;
        return Parent_type::Resize(size);
    }

    void CAutoTasksDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool>)
    {
        m_okButton->SetFocus();
    }

    void CAutoTasksDialog::OnPreDock(Rect& rect)
    {
        const int minWidth = 54;
        if (rect.size.width < minWidth)
        {
            int diff = minWidth - rect.size.width;
            rect.position.x -= diff / 2;
            rect.size.width = minWidth;
        }
    }

    bool CAutoTasksDialog::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        if (evt.keyEvent.valid)
        {
            auto pool = GetPool();
            auto focused = pool ? pool->GetFocus() : nullptr;
            const std::shared_ptr<CWindow> order[] = { m_analyzeCheck, m_symbolsCheck, m_okButton, m_cancelButton };
            const int count = (int)(sizeof(order) / sizeof(order[0]));
            int current = -1;
            for (int i = 0; i < count; ++i)
            {
                if (order[i] == focused)
                {
                    current = i;
                }
            }
            const bool shift = (evt.keyState.state & evt.keyState.AnyShift) != 0;
            switch (evt.keyEvent.virtualKey)
            {
            case VirtualKey::Tab:
            case VirtualKey::Down:
            case VirtualKey::Up:
            {
                const bool back = evt.keyEvent.virtualKey == VirtualKey::Up ||
                    (evt.keyEvent.virtualKey == VirtualKey::Tab && shift);
                const int next = current < 0 ? 0 : (current + (back ? count - 1 : 1)) % count;
                order[next]->SetFocus();
                return true;
            }
            case VirtualKey::Enter:
                // a checkbox passes Enter on: the dialog confirms
                if (focused != m_cancelButton && focused != m_okButton)
                {
                    Confirm();
                    return true;
                }
                break;
            default:
                break;
            }
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
}
