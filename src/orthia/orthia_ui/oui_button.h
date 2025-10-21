#pragma once

#include "oui_label.h"
#include "oui_win_styles.h"

namespace oui
{

    class CButton:public oui::MouseFocusable<CWindow>
    {
        using Parent_type = oui::MouseFocusable<CWindow>;

        std::shared_ptr<ButtonColorProfile> m_colorProfile;
        std::function<String()> m_getText;
        static String m_chunk, m_chunkText;

    protected:

    public:
        CButton(std::shared_ptr<ButtonColorProfile> colorProfile, std::function<String()> getText);
        ~CButton();

        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        String GetText() const;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
    };

}
