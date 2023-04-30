#pragma once

#include "oui_window.h"

namespace oui
{
    class CLabel:public CWindow
    {
        std::shared_ptr<DialogColorProfile> m_colorProfile;
        std::function<String()> m_getText;

        static String m_chunk;

        Rect m_lastRect;
        Point m_lastMouseMovePoint;
    public:
        CLabel(std::shared_ptr<DialogColorProfile> colorProfile, std::function<String()> getText);
        void DoPaint(const Rect& rect, DrawParameters& parameters) override;
        String GetText() const;
        bool HandleMouseEvent(const Rect& rect, InputEvent& evt) override;
    };

}