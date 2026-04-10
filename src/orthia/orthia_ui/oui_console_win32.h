#ifdef OUI_SYS_WINDOWS

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

    class CConsoleDrawAdapter:Noncopyable
    {
        Size m_size;
        std::vector<CHAR_INFO> m_buffer;
        mutable CConsole* m_console = 0;
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

        int PaintText(const Point& position,
            Color textColor,
            Color textBgColor,
            const String& text,
            String hotkeySymbol = String(),
            Color highlightTextColor = Color(),
            Color highlightTextBgColor = Color()
            );

        void PaintBorder(const Rect & rect, 
            Color textColor,
            Color textBgColor,
            BorderStyle style);

        enum class ScrollMarkType
        {
            Left,
            Right
        };
        void PaintScrollMark(const Point& position, ScrollMarkType type, Color textColor, Color textBgColor);

        // main interface
        CConsole* GetConsole() { return m_console; }
        Size GetSize() const { return m_size; }

        void StartDraw(Size size, CConsole* console);
        void FinishDraw();

        void CopyRectWindow(const Rect & rect, const Point& targetPosition, CConsoleDrawAdapter& consoleOut) const;
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

#endif
