#pragma once

#include "oui_window.h"

namespace oui
{
    class CLabel:public CWindow
    {
        std::shared_ptr<LabelColorProfile> m_colorProfile;
        std::function<String()> m_getText;

        static String m_chunk;

    public:
        CLabel(std::shared_ptr<LabelColorProfile> colorProfile, std::function<String()> getText);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        String GetText() const;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt, MouseEventContext& mouseEventContext) override;
        void SetColorProfile(std::shared_ptr<LabelColorProfile> colorProfile);
    };

}