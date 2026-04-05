#ifdef OUI_SYS_POSIX

#include <termios.h>

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
        struct TerminalCell
        {
            std::string character = " ";
            Color fgColor = {200, 200, 200};
            Color bgColor = {0, 0, 0};
            bool operator==(const TerminalCell& o) const
            {
                return character == o.character &&
                       fgColor == o.fgColor &&
                       bgColor == o.bgColor;
            }
            bool operator!=(const TerminalCell& o) const { return !(*this == o); }
        };

        Size m_size;
        std::vector<TerminalCell> m_backBuffer;
        std::vector<TerminalCell> m_frontBuffer;
        bool m_fullRedraw = true;
        CConsole* m_console = nullptr;

        static std::string ExtractUtf8Char(const std::string& str, size_t& i);
        static void AppendMoveCursor(std::string& out, int x, int y);
        static void AppendAnsiColor(std::string& out, Color fg, Color bg);

        void PaintCell(int x, int y, const std::string& ch, Color fg, Color bg);

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
        CConsole* GetConsole() { return m_console; }
        Size GetSize() const { return m_size; }

        void StartDraw(Size size, CConsole* console);
        void FinishDraw();

        void CopyRectWindow(const Rect & rect, const Point& targetPosition, CConsoleDrawAdapter& consoleOut) const;
    };

    class CConsoleStateSaver:Noncopyable
    {
        struct termios m_originalTermios;
    public:
        CConsoleStateSaver();
        ~CConsoleStateSaver();
    };

}

#endif
