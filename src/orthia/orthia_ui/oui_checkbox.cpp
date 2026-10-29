#include "oui_checkbox.h"

namespace oui
{
    String CCheckBox::m_chunk, CCheckBox::m_chunkText;

    CCheckBox::CCheckBox(std::shared_ptr<ButtonColorProfile> colorProfile, std::function<String()> getText, bool checked)
        :
        m_colorProfile(colorProfile),
        m_getText(getText),
        m_checked(checked)
    {
    }
    CCheckBox::~CCheckBox()
    {
    }
    void CCheckBox::SetChecked(bool checked)
    {
        if (m_checked != checked)
        {
            m_checked = checked;
            Invalidate();
        }
    }
    void CCheckBox::Toggle()
    {
        m_checked = !m_checked;
        Invalidate();
        if (m_onChange)
        {
            m_onChange(m_checked);
        }
    }
    void CCheckBox::SetChangeHandler(std::function<void(bool)> handler)
    {
        m_onChange = std::move(handler);
    }
    String CCheckBox::GetText() const
    {
        String text = m_getText ? m_getText() : String();
        if (auto console = GetConsole())
        {
            console->FilterOrReplaceUnreadableSymbols(text);
        }
        return text;
    }
    void CCheckBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        const int width = absClientRect.size.width;
        if (width <= 0)
        {
            return;
        }

        // "[x] text", the rest of the row blank
        m_chunk.native.clear();
        m_chunk.native.push_back(String::char_type('['));
        m_chunk.native.push_back(m_checked ? String::char_type('x') : String::symSpace);
        m_chunk.native.push_back(String::char_type(']'));
        m_chunk.native.push_back(String::symSpace);
        if ((int)m_chunk.native.size() > width)
        {
            m_chunk.native.resize(width);
        }
        m_chunkText = GetText();
        const auto info = console->GetSymbolsAnalyzer().CutVisibleString(m_chunkText.native, width - (int)m_chunk.native.size());
        const int visible = (int)m_chunk.native.size() + info.visibleSize;
        m_chunk.native += m_chunkText.native;
        if (visible < width)
        {
            m_chunk.native.append(width - visible, String::symSpace);
        }

        auto state = &m_colorProfile->normal;
        if (IsMouseOn())
        {
            state = &m_colorProfile->mouseHighlight;
        }
        else if (IsFocused())
        {
            state = &m_colorProfile->selected;
        }
        parameters.console.PaintText(absClientRect.position,
            state->text,
            state->background,
            m_chunk.native);
    }
    bool CCheckBox::HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext)
    {
        Invalidate(false);
        if (evt.mouseEvent.button == MouseButton::Left &&
            evt.mouseEvent.state == MouseState::Released)
        {
            Toggle();
        }
        return true;
    }
    bool CCheckBox::IsToggleKey(const InputEvent& evt)
    {
        return evt.keyEvent.valid &&
            evt.keyEvent.virtualKey == VirtualKey::None &&
            evt.keyEvent.rawText.native.size() == 1 &&
            evt.keyEvent.rawText.native[0] == String::symSpace &&
            !(evt.keyState.state & (evt.keyState.AnyCtrl | evt.keyState.AnyAlt));
    }
    bool CCheckBox::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        if (IsToggleKey(evt))
        {
            Toggle();
            return true;
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
}
