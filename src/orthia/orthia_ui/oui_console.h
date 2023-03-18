#pragma once

#include "oui_base.h"
#include "oui_color.h"
#include "windows.h"

namespace oui
{
    class CConsole
    {
        std::unordered_map<Color, int, ColorHash> m_colorCache;

        int TranslateColor(const Color& color);

    public:
        CConsole();
        void Init();
        Size GetSize();
        void FixupAfterResize();
        void PaintRect(const Rect& rect, 
            Color background,
            bool keepText);
        void HideCursor();
        int TranslateColorEx(const Color& color, bool background);
    };

    class CConsoleDrawAdapter:Noncopyable
    {
        Size m_size;
        std::vector<CHAR_INFO> m_buffer;
        CConsole* m_console = 0;
    public:
        void PaintRect(const Rect& rect,
            Color background,
            bool keepText);

        void StartDraw(Size size, 
            CConsole* console);
        void FinishDraw();
    };
    class CConsoleStateSaver:Noncopyable
    {
        BOOL m_restoreCursorInfo = FALSE;
        CONSOLE_CURSOR_INFO m_cursorInfo = { 0, 0 };

        BOOL m_restoreScreenInfo = FALSE;
        CONSOLE_SCREEN_BUFFER_INFOEX m_screenInfo;

        BOOL m_restoreData = FALSE;

        std::vector<CHAR_INFO> m_buffer;
    public:
        CConsoleStateSaver();
        ~CConsoleStateSaver();
    };
}