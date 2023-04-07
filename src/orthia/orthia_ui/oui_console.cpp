#include "oui_console.h"
#include "windows.h"

#undef min

namespace oui
{
#define OUI_ENABLE_VIRTUAL_TERMINAL_INPUT       0x0200


    struct ColorMap
    {
        Color color;
        int value;
    };
    static ColorMap g_defaultPalette[] =
    {
        ColorMap{ ColorBlack(), 0},                                                       // black
        ColorMap{ ColorBlue(), FOREGROUND_BLUE},                                          // blue
        ColorMap{ ColorGreen(), FOREGROUND_GREEN},                                        // green
        ColorMap{ ColorCyan(), FOREGROUND_BLUE | FOREGROUND_GREEN},                       // cyan
        ColorMap{ ColorRed(), FOREGROUND_RED},                                            // red
        ColorMap{ ColorMagenta(), FOREGROUND_RED | FOREGROUND_BLUE},                      // magenta
        ColorMap{ ColorYellow(), FOREGROUND_RED | FOREGROUND_GREEN},                      // yellow
        ColorMap{ ColorWhite(), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE},     // white

        ColorMap{ ColorGray(), FOREGROUND_INTENSITY},                                     // bright black, lol
        ColorMap{ ColorBrightBlue(), FOREGROUND_BLUE | FOREGROUND_INTENSITY},                    // bright blue
        ColorMap{ ColorBrightGreen(), FOREGROUND_GREEN | FOREGROUND_INTENSITY},                  // bright green
        ColorMap{ ColorBrightCyan(), FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY}, // bright cyan
        ColorMap{ ColorBrightRed(), FOREGROUND_RED | FOREGROUND_INTENSITY},                     // bright red
        ColorMap{ ColorBrightMagenta(), FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY},   // bright magenta
        ColorMap{ ColorBrightYellow(), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY},  // bright yellow
        ColorMap{ ColorBrightWhite(), FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY}  // bright white
    };

    static int NativeTranslateColor(const Color& color)
    {
        int minDifference = INT_MAX;
        int position = 0;

        for (int i = 0; i < 16; ++i)
        {
            Color cur = g_defaultPalette[i].color;
            int currentDifference = (int)std::sqrt(
                std::pow(std::abs((int)cur.Blue() - (int)color.Blue()), 2) +
                std::pow(std::abs((int)cur.Red() - (int)color.Red()), 2) +
                std::pow(std::abs((int)cur.Green() - (int)color.Green()), 2)
            );
            if (currentDifference < minDifference)
            {
                position = i;
                minDifference = currentDifference;
            }
        }
        return g_defaultPalette[position].value;
    }
    CConsole::CConsole()
    {

    }
    void CConsole::SetPalette(std::array<COLORREF, 16>& colors)
    {
        CConsoleStateSaver saver;
        std::copy(colors.begin(), colors.end(), saver.GetScreenInfo().ColorTable);
    }
    void CConsole::SetDefaultPalette()
    {
        std::array<COLORREF, 16> colors;
        for (int i = 0; i < 16; ++i)
        {
            auto& color = g_defaultPalette[i].color;
            colors[i] = RGB(color.Red(), color.Green(), color.Blue());
        }
        SetPalette(colors);
    }
    HWND CConsole::GetRealWindow()
    {
        HWND hWnd = GetConsoleWindow();

        wchar_t className[MAX_PATH];
        int size = GetClassName(hWnd, className, (int)std::size(className));
        if (size)
        {
            if (wcsncmp(className, L"PseudoConsoleWindow", size) == 0)
            {
                if (auto owner = GetWindow(hWnd, GW_OWNER))
                    return owner;
            }
        }
        return hWnd;
    }

    void CConsole::Init()
    {
        SetConsoleCP(CP_UTF8);
        SetConsoleOutputCP(CP_UTF8);
        setlocale(LC_ALL, ".utf8");

        DWORD mode = 0;
        auto inputHandle = GetStdHandle(STD_INPUT_HANDLE);
        if (GetConsoleMode(inputHandle, &mode))
        {
            mode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | OUI_ENABLE_VIRTUAL_TERMINAL_INPUT);
            mode |= ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT;
            SetConsoleMode(inputHandle, mode);
        }
        FixupAfterResize();
        SetDefaultPalette();

        auto wnd = GetRealWindow();
        if (IsWindowVisible(wnd))
        {
            SendMessage(wnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
        }
    }
    void CConsole::HideCursor()
    {
        CONSOLE_CURSOR_INFO info;
        info.bVisible = FALSE;
        info.dwSize = 100;
        SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    }
    Size CConsole::GetSize()
    {
        Size result;
        CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;

        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), 
                                &screenBufferInfo))
        {
           return Size{ screenBufferInfo.srWindow.Right - screenBufferInfo.srWindow.Left + 1,
                        screenBufferInfo.srWindow.Bottom - screenBufferInfo.srWindow.Top + 1 };
        }
        return result;
    }
    void CConsole::FixupAfterResize()
    {
        const auto size = GetSize();
        const COORD sizeToPass = {(SHORT)size.width, (SHORT)size.height };
        BOOL res = SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE),
            sizeToPass);
        int cccc = 0;
    }

    int CConsole::TranslateColor(const Color& color)
    {
        auto it = m_colorCache.find(color);
        if (it != m_colorCache.end())
        {
            return it->second;
        }

        auto conColor = NativeTranslateColor(color);
        m_colorCache[color] = conColor;
        return conColor;
    }
    int CConsole::TranslateColorEx(const Color& color, bool background)
    {
        int result = TranslateColor(color);
        if (background)
        {
            result <<= 4;
        }
        return result;
    }
    void CConsole::PaintRect(const Rect& rect,
        Color background,
        bool keepText)
    {
        int consoleColor = TranslateColorEx(background, true);

        auto hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        
        COORD dwWriteCoord = { (SHORT)rect.position.x, (SHORT)rect.position.y };
        int linesCount = rect.size.height;
        DWORD rowLength = rect.size.width;
        for (int i = 0; i < linesCount; ++i)
        {
            DWORD charsWritten = 0;
            if (!keepText)
            {
                FillConsoleOutputCharacter(hConsole, L' ', rowLength, dwWriteCoord, &charsWritten);
            }
            FillConsoleOutputAttribute(hConsole, consoleColor, rowLength, dwWriteCoord, &charsWritten);
            ++dwWriteCoord.Y;
        }
    }

    // CConsoleDrawAdapter
    const wchar_t g_symsOfSeparatorThick[] = L"─╟╢";
    const wchar_t g_symsOfSeparatorThin[] = L"─├┤";
    const wchar_t g_symsOfBorderThick[] = L"╔═╗║╚═╝";
    const wchar_t g_symsOfBorderThin[] = L"┌─┐│└─┘";

    PanelBorderSymbols GetPanelBorderSymbols()
    {
        PanelBorderSymbols symbols;
        symbols.vertical = g_symsOfBorderThin[3];
        symbols.horizontal = g_symsOfBorderThin[1];
        symbols.left_top = g_symsOfBorderThin[0];
        symbols.right_top = g_symsOfBorderThin[2];
        symbols.left_bottom = g_symsOfBorderThin[4];
        symbols.right_bottom = g_symsOfBorderThin[6];
        return symbols;
    }

    void CConsoleDrawAdapter::PaintMenuSeparator(const Point& position,
        int width,
        Color textColor,
        Color textBgColor,
        BorderStyle style)
    {
        auto syms = g_symsOfSeparatorThick;
        if (style == BorderStyle::Thin)
        {
            syms = g_symsOfSeparatorThin;
        }
        if (width <= 0)
        {
            return;
        }
        if (!m_separator.empty())
        {
            m_separator[0] = syms[0];
            m_separator[m_separator.size() - 1] = syms[0];
        }
        m_separator.resize(width, syms[0]);
        m_separator[0] = syms[1];
        m_separator[m_separator.size() - 1] = syms[2];
        PaintText(position, textColor, textBgColor, m_separator);
    }
    void CConsoleDrawAdapter::PaintText(const Point& position,
        Color textColor,
        Color textBgColor,
        const String& text,
        String::char_type hotkeySymbol,
        Color highlightTextColor,
        Color highlightTextBgColor)
    {
        if (position.x < 0 || position.y < 0)
        {
            return;
        }
        String::char_type hotkeySymbolTmp = hotkeySymbol;
        if (position.x >= m_size.width)
        {
            return;
        }
        if (position.y >= m_size.height)
        {
            return;
        }
        CHAR_INFO* rawData = m_buffer.data();

        const int frontColor = m_console->TranslateColorEx(textColor, false);
        const int backColor = m_console->TranslateColorEx(textBgColor, true);
        const int normalAttributes = frontColor | backColor;

        int highFrontColor = 0; 
        int highBackColor = 0; 
        int highAttributes = 0; 

        int currentAttributes = normalAttributes;

        if (hotkeySymbol)
        {
            highFrontColor = m_console->TranslateColorEx(highlightTextColor, false);
            highBackColor = m_console->TranslateColorEx(highlightTextBgColor, true);
            highAttributes = highFrontColor | highBackColor;
        }

        CHAR_INFO* lineData = rawData + (m_size.width * position.y);
        int xend = std::min((int)m_size.width, (int)text.native.size() + position.x);

        auto textPtr = text.native.c_str();
        for (CHAR_INFO* p = lineData + position.x, *p_end = lineData + xend; p < p_end; ++p, ++textPtr)
        {
            if (!*textPtr)
            {
                break;
            }
            if (*textPtr == hotkeySymbolTmp)
            {
                hotkeySymbolTmp = 0;
                --p;
                currentAttributes = highAttributes;
                continue;
            }
            p->Attributes = currentAttributes;
            p->Char.UnicodeChar = *textPtr;
            currentAttributes = normalAttributes;
            hotkeySymbolTmp = hotkeySymbol;
        }
    }
    void CConsoleDrawAdapter::PaintBorder(const Rect& rect_in,
        Color textColor,
        Color textBgColor,
        BorderStyle style)
    {
        auto syms = g_symsOfBorderThick;
        if (style == BorderStyle::Thin)
        {
            syms = g_symsOfBorderThin;
        }

        if (rect_in.size.height <= 0 || rect_in.size.width <= 0)
        {
            return;
        }
        Rect rect = rect_in;
        if (rect.size.width > m_size.width)
        {
            rect.size.width = m_size.width;
        }
        if (rect.size.height > m_size.height)
        {
            rect.size.height = m_size.height;
        }
        if (rect.position.x >= m_size.width)
        {
            return;
        }
        if (rect.position.y >= m_size.height)
        {
            return;
        }

        CHAR_INFO* rawData = m_buffer.data();

        int consoleColor = m_console->TranslateColorEx(textBgColor, true) | m_console->TranslateColorEx(textColor, false);
          
        DWORD rowLength = rect.size.width;

        int xend = std::min((int)m_size.width, (int)rowLength + rect.position.x);
        int linesCount = std::min(rect.size.height, (int)m_size.height - rect.position.y);

        CHAR_INFO* lineData = rawData + m_size.width * rect.position.y;

        lineData[rect.position.x].Char.UnicodeChar = syms[0];
        lineData[rect.position.x].Attributes = consoleColor;
        for (int u = rect.position.x + 1; u < xend - 1; ++u)
        {
            lineData[u].Char.UnicodeChar = syms[1];
            lineData[u].Attributes = consoleColor;
        }

        lineData[xend - 1].Attributes = consoleColor;
        lineData[xend - 1].Char.UnicodeChar = syms[2];

        lineData += m_size.width;
        for (int i = 0; i < linesCount - 2; ++i, lineData += m_size.width)
        {
            DWORD charsWritten = 0;

            lineData[rect.position.x].Char.UnicodeChar = syms[3];
            lineData[rect.position.x].Attributes = consoleColor;

            lineData[xend - 1].Char.UnicodeChar = syms[3];
            lineData[xend - 1].Attributes = consoleColor;
        }

        lineData[rect.position.x].Char.UnicodeChar = syms[4];
        lineData[rect.position.x].Attributes = consoleColor;
        for (int u = rect.position.x + 1; u < xend - 1; ++u)
        {
            lineData[u].Char.UnicodeChar = syms[5];
            lineData[u].Attributes = consoleColor;
        }
        lineData[xend - 1].Char.UnicodeChar = syms[6];
        lineData[xend - 1].Attributes = consoleColor;
    }

    void CConsoleDrawAdapter::PaintRect(const Rect& rect_in,
        Color background,
        bool keepText)
    {
        if (rect_in.size.height <= 0 || rect_in.size.width <= 0)
        {
            return;
        }
        Rect rect = rect_in;
        if (rect.size.width > m_size.width)
        {
            rect.size.width = m_size.width;
        }
        if (rect.size.height > m_size.height)
        {
            rect.size.height = m_size.height;
        }
        if (rect.position.x >= m_size.width)
        {
            return;
        }
        if (rect.position.y >= m_size.height)
        {
            return;
        }

        CHAR_INFO* rawData = m_buffer.data();

        int consoleColor = m_console->TranslateColorEx(background, true);

        int linesCount = std::min(rect.size.height, (int)m_size.height - rect.position.y);
        DWORD rowLength = rect.size.width;

        int xend = std::min((int)m_size.width, (int)rowLength + rect.position.x);

        CHAR_INFO* lineData = rawData + m_size.width * rect.position.y;
        for (int i = 0; i < linesCount; ++i, lineData += m_size.width)
        {
            DWORD charsWritten = 0;

            if (keepText)
            {
                for (int u = rect.position.x; u < xend; ++u)
                {
                    lineData[u].Attributes = consoleColor;
                }
            }
            else
            {
                for (int u = rect.position.x; u < xend; ++u)
                {
                    lineData[u].Char.UnicodeChar = L' ';
                    lineData[u].Attributes = consoleColor;
                }
            }
        }
    }

    void CConsoleDrawAdapter::StartDraw(Size size,
        CConsole* console)
    {
        m_size = size;
        m_console = console;
        CHAR_INFO chInfo;
        chInfo.Attributes = 0;
        chInfo.Char.UnicodeChar = L' ';
        m_buffer.resize(m_size.height * m_size.width, chInfo);
    }
    void CConsoleDrawAdapter::FinishDraw()
    {
        COORD offset = { (SHORT)0, (SHORT)0};
        COORD size = { (SHORT)m_size.width, (SHORT)m_size.height };
        SMALL_RECT region;
        region.Top = 0;
        region.Left = 0;
        region.Right = m_size.width - 1;
        region.Bottom = m_size.height - 1;
        BOOL res = WriteConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),
            m_buffer.data(),
            size,
            offset,
            &region);

        int cccc = 1;
    }
    
    // CConsoleStateSaver
    CConsoleStateSaver::CConsoleStateSaver()
    {
        m_screenInfo.cbSize = sizeof(m_screenInfo);
        m_restoreScreenInfo = GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &m_screenInfo);
        m_restoreCursorInfo = GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &m_cursorInfo);

        if (m_restoreScreenInfo)
        {
            m_buffer.resize(m_screenInfo.dwSize.X * m_screenInfo.dwSize.Y);

            COORD bufferCoord = { (SHORT)0, (SHORT)0 };
            SMALL_RECT region;
            region.Top = 0;
            region.Left = 0;
            region.Right = m_screenInfo.dwSize.X - 1;
            region.Bottom = m_screenInfo.dwSize.Y - 1;
            m_restoreData = ReadConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),
                m_buffer.data(),
                m_screenInfo.dwSize,
                bufferCoord,
                &region);
        }
    }
    CConsoleStateSaver::~CConsoleStateSaver()
    {
        if (m_restoreScreenInfo)
        {
            if (SetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &m_screenInfo))
            {
                SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &m_screenInfo.srWindow);
                if (m_restoreData)
                {
                    COORD bufferCoord = { (SHORT)0, (SHORT)0 };
                    SMALL_RECT region;
                    region.Top = 0;
                    region.Left = 0;
                    region.Right = m_screenInfo.dwSize.X - 1;
                    region.Bottom = m_screenInfo.dwSize.Y - 1;
                    m_restoreData = WriteConsoleOutput(GetStdHandle(STD_OUTPUT_HANDLE),
                        m_buffer.data(),
                        m_screenInfo.dwSize,
                        bufferCoord,
                        &region);
                }
            }
        }
        if (m_restoreCursorInfo)
        {
            SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &m_cursorInfo);
        }        
    }
}