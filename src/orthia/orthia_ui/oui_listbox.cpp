#include "oui_listbox.h"

namespace oui
{
    CListBox::CListBox(std::shared_ptr<DialogColorProfile> colorProfile, IListBoxOwner* owner)
        :
            m_colorProfile(colorProfile),
            m_owner(owner)
    {
        SetBorderStyle(oui::BorderStyle::Thin);
        SetColors(colorProfile->listBox.borderColor, oui::Color());
    }
    void CListBox::DoPaint(const Rect& rect, DrawParameters& parameters)
    {
        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        const auto absClientRect = GetAbsoluteClientRect(this, rect);
        Parent_type::DoPaint(rect, parameters);
;    }
    void CListBox::OnResize()
    {
        Parent_type::OnResize();
    }
    void CListBox::Destroy()
    {
        m_owner->CancelAllQueries();
        Parent_type::Destroy();
    }
    bool CListBox::HandleMouseEvent(const Rect& rect, InputEvent& evt)
    {
        return true;
    }
}