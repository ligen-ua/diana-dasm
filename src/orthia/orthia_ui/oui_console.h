#pragma once

#include "oui_base.h"
#include "oui_color.h"
#include "oui_string.h"

#include "windows.h"

namespace oui
{
    class CConsole
    {
        std::unordered_map<Color, int, ColorHash> m_colorCache;

        int TranslateColor(const Color& color);
        void SetPalette(std::array<COLORREF, 16>& colors);
        void SetDefaultPalette();
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
        HWND GetRealWindow();
    };

    struct PanelBorderSymbols
    {
        oui::String::char_type vertical;
        oui::String::char_type horizontal;
        oui::String::char_type left_top;
        oui::String::char_type right_top;
        oui::String::char_type left_bottom;
        oui::String::char_type right_bottom;
    };

    PanelBorderSymbols GetPanelBorderSymbols();

    class CConsoleDrawAdapter:Noncopyable
    {
        Size m_size;
        std::vector<CHAR_INFO> m_buffer;
        CConsole* m_console = 0;
        std::wstring m_separator;
    public:
        void PaintMenuSeparator(const Point& position,
            int width,
            Color textColor,
            Color textBgColor,
            BorderStyle style);

        void PaintRect(const Rect& rect,
            Color background,
            bool keepText);

        void PaintText(const Point& position,
            Color textColor,
            Color textBgColor,
            const String& text,
            String::char_type hotkeySymbol = 0,
            Color highlightTextColor = Color(),
            Color highlightTextBgColor = Color()
            );

        void PaintBorder(const Rect & rect, 
            Color textColor,
            Color textBgColor,
            BorderStyle style);

        // main
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

        CONSOLE_SCREEN_BUFFER_INFOEX& GetScreenInfo() {
            return m_screenInfo;
        }
    };
}