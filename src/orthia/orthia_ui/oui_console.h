#pragma once

#include "oui_base.h"
#include "oui_color.h"
#include "oui_string.h"

namespace oui
{
    class CConsole
    {
        HWND m_consoleWindow = 0;
        std::unordered_map<Color, int, ColorHash> m_colorCache;
        bool m_newTerminal = false;
        std::unique_ptr<ISymbolsAnalyzer> m_symbolsAnalyzer;

        int TranslateColor(const Color& color);
        void SetPalette(std::array<COLORREF, 16>& colors);
        void SetDefaultPalette();
        short GetYDifference() const;
        void DetectVersion();
    public:
        CConsole();
        void SetTitle(const String& caption);
        void FilterOrReplaceUnreadableSymbols(String& data);
        void ReplaceWideSymbols(String& data);
        ISymbolsAnalyzer& GetSymbolsAnalyzer();
        void Init();
        Size GetSize();
        void FixupAfterResize();
        void PaintRect(const Rect& rect, 
            Color background,
            bool keepText);
        void ShowCursor();
        void HideCursor();
        int TranslateColorEx(const Color& color, bool background);
        HWND GetRealWindow();

        void SetCursorPositon(const Point& pt);
        Point GetCursorPositon();

        bool CopyTextToClipboard(const String& text);
        String PasteTextFromClipboard();

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