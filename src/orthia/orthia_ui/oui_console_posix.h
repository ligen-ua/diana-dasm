#ifdef OUI_SYS_POSIX

namespace oui
{
    class CConsole
    {
        std::unique_ptr<ISymbolsAnalyzer> m_symbolsAnalyzer;
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

        void SetCursorPositon(const Point& pt);

        bool CopyTextToClipboard(const String& text);
        String PasteTextFromClipboard();
    };


    class CConsoleDrawAdapter:Noncopyable
    {

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
            String::char_type hotkeySymbol = 0,
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
        CConsole* GetConsole();
        Size GetSize() const;

        void StartDraw(Size size, CConsole* console);
        void FinishDraw();

        void CopyRectWindow(const Rect & rect, const Point& targetPosition, CConsoleDrawAdapter& consoleOut) const;
    };
    class CConsoleStateSaver:Noncopyable
    {
    public:
        CConsoleStateSaver();
        ~CConsoleStateSaver();
    };

}

#endif
