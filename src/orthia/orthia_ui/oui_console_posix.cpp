#include "oui_console.h"
#include "oui_symbols_posix.h"
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <cerrno>
#include <cstdlib>
#include <time.h>
#include <poll.h>

namespace oui
{

static void WriteToStdout(const void* data, size_t size)
{
    write(STDOUT_FILENO, data, size);
}

// ---------------------------------------------------------
// UTF-8 box-drawing character tables (3-byte UTF-8 sequences)
// Index layout mirrors the Win32 g_symsOfBorder* arrays:
//   [0]=top-left  [1]=top  [2]=top-right
//   [3]=side      [4]=bot-left  [5]=bot  [6]=bot-right
// ---------------------------------------------------------
static const char* g_boxThick[] = {
    "\xE2\x95\x94",  // ╔ U+2554
    "\xE2\x95\x90",  // ═ U+2550
    "\xE2\x95\x97",  // ╗ U+2557
    "\xE2\x95\x91",  // ║ U+2551
    "\xE2\x95\x9A",  // ╚ U+255A
    "\xE2\x95\x90",  // ═ U+2550
    "\xE2\x95\x9D",  // ╝ U+255D
};
static const char* g_boxThin[] = {
    "\xE2\x94\x8C",  // ┌ U+250C
    "\xE2\x94\x80",  // ─ U+2500
    "\xE2\x94\x90",  // ┐ U+2510
    "\xE2\x94\x82",  // │ U+2502
    "\xE2\x94\x94",  // └ U+2514
    "\xE2\x94\x80",  // ─ U+2500
    "\xE2\x94\x98",  // ┘ U+2518
};

// Separator tables: [0]=fill  [1]=left-cap  [2]=right-cap
static const char* g_sepThick[] = {
    "\xE2\x94\x80",  // ─ U+2500
    "\xE2\x95\x9F",  // ╟ U+255F
    "\xE2\x95\xA2",  // ╢ U+2562
};
static const char* g_sepThin[] = {
    "\xE2\x94\x80",  // ─ U+2500
    "\xE2\x94\x9C",  // ├ U+251C
    "\xE2\x94\xA4",  // ┤ U+2524
};

// ---------------------------------------------------------
// CConsole
// ---------------------------------------------------------

CConsole::CConsole() {}

void CConsole::Init()
{
    // Enter alternate screen buffer, clear it, hide cursor
    const char* seq = "\x1B[?1049h\x1B[2J\x1B[?25l";
    WriteToStdout(seq, strlen(seq));

    // Switch terminal to raw mode so keys arrive immediately without echo.
    // CConsoleStateSaver (created before Init() is called) already saved the
    // original termios and will restore it on destruction.
    struct termios raw;
    if (tcgetattr(STDIN_FILENO, &raw) == 0)
    {
        raw.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(unsigned)OPOST;
        raw.c_cflag |= CS8;
        raw.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 1;  // 100 ms read timeout
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    }

    m_symbolsAnalyzer.reset(new CPosixSymbolsAnalyzer());
}

void CConsole::SetTitle(const String& caption)
{
    std::string seq = "\x1B]0;" + caption.native + "\x07";
    WriteToStdout(seq.data(), seq.size());
}

ISymbolsAnalyzer& CConsole::GetSymbolsAnalyzer()
{
    return *m_symbolsAnalyzer;
}

void CConsole::FilterOrReplaceUnreadableSymbols(String& data)
{
    FilterUnreadableSymbols(data.native);
    // No wide-symbol replacement needed on POSIX — terminals handle UTF-8 natively.
}

void CConsole::ReplaceWideSymbols(String& /*data*/)
{
    // No-op on POSIX: terminals handle multi-column Unicode via cursor positioning.
}

int CConsole::TranslateColorEx(const Color& /*color*/, bool /*background*/)
{
    // Not used on POSIX — the draw adapter writes 24-bit ANSI colour sequences directly.
    return 0;
}

Size CConsole::GetSize()
{
    Size size;
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
    {
        size.width  = w.ws_col;
        size.height = w.ws_row;
    }
    return size;
}

void CConsole::FixupAfterResize() {}

void CConsole::PaintRect(const Rect& /*rect*/, Color /*background*/, bool /*keepText*/)
{
    // On POSIX all painting goes through CConsoleDrawAdapter; this is a no-op.
}

void CConsole::ShowCursor()
{
    const char* seq = "\x1B[?25h";
    WriteToStdout(seq, strlen(seq));
}

void CConsole::HideCursor()
{
    const char* seq = "\x1B[?25l";
    WriteToStdout(seq, strlen(seq));
}

void CConsole::SetCursorPositon(const Point& pt)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1B[%d;%dH", pt.y + 1, pt.x + 1);
    WriteToStdout(buf, len);
}

static void CloseFdsFrom(int lowfd)
{
    DIR* dir = opendir("/proc/self/fd");
    if (dir) {
        int dirfd_ = dirfd(dir);
        struct dirent* e;
        while ((e = readdir(dir))) {
            char* end;
            long fd = strtol(e->d_name, &end, 10);
            if (*end == '\0' && fd >= lowfd && fd != dirfd_)
                close(static_cast<int>(fd));
        }
        closedir(dir);
    } else {
        // Fallback: sysconf may return up to 1,048,576 on Linux —
        // still safe, just slightly slower.
        int maxfd = static_cast<int>(sysconf(_SC_OPEN_MAX));
        for (int fd = lowfd; fd < maxfd; ++fd)
            close(fd);
    }
}
static bool TryClipboardTool(const char* const* argv,
                              const char*        data,
                              size_t             len,
                              int                timeoutMs = 2000)
{
    // O_CLOEXEC ensures both ends are closed automatically at execvp,
    // so they never leak into the clipboard tool's fd table.
    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) != 0)
        return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return false;
    }

    if (pid == 0) {
        // ---- child -------------------------------------------------------

        // Wire the read end to stdin.
        // dup2 strips O_CLOEXEC from the new fd, so STDIN_FILENO survives exec.
        close(pfd[1]);
        if (dup2(pfd[0], STDIN_FILENO) < 0) _exit(1);
        close(pfd[0]);

        // Silence stderr; open with O_CLOEXEC — dup2 keeps STDERR_FILENO open.
        int dn = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (dn >= 0) { dup2(dn, STDERR_FILENO); close(dn); }

        // Close every fd above STDERR_FILENO.
        // O_CLOEXEC already handles our own pipe ends, but third-party libraries
        // may have opened fds without O_CLOEXEC; this sweeps them all up.
        CloseFdsFrom(STDERR_FILENO + 1);

        execvp(argv[0], const_cast<char* const*>(argv));
        _exit(127); // tool not found / exec failed
    }

    // ---- parent ----------------------------------------------------------

    // Close the read end; we only write.
    close(pfd[0]);

    // Stream the clipboard text into the tool.
    {
        const char* p   = data;
        size_t      rem = len;
        while (rem > 0) {
            ssize_t n = write(pfd[1], p, rem);
            if (n <= 0) break;
            p += n; rem -= n;
        }
    }
    close(pfd[1]); // EOF — tells the tool we're done

    // Poll for child exit in 50 ms steps up to timeoutMs.
    const struct timespec kStep = { 0, 50'000'000L }; // 50 ms
    const int             kMaxIter = timeoutMs / 50;

    for (int i = 0; i < kMaxIter; ++i) {
        int   status = 0;
        pid_t r      = waitpid(pid, &status, WNOHANG);

        if (r == pid)   // child exited
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;

        if (r < 0 && errno != EINTR)
            break;      // unexpected error

        nanosleep(&kStep, nullptr);
    }

    // Timed out (e.g. xclip waiting for a paste request).
    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return false;
}

bool CConsole::CopyTextToClipboard(const String& text)
{
    const char* data = text.native.data();
    size_t      len  = text.native.size();

    // 1. Wayland (Ubuntu 24.04 default session).
    //    Package: sudo apt install wl-clipboard
    //    "--" prevents text starting with "-" being misread as a flag.
    {
        const char* argv[] = { "wl-copy", "--", nullptr };
        if (TryClipboardTool(argv, data, len)) return true;
    }

    // 2. X11 / XWayland.
    //    Package: sudo apt install xsel
    //    xsel forks a persistent clipboard daemon and exits quickly — safe to wait on.
    {
        const char* argv[] = { "xsel", "--clipboard", "--input", nullptr };
        if (TryClipboardTool(argv, data, len)) return true;
    }

    // 3. X11 last resort.
    //    Package: sudo apt install xclip
    //    xclip must remain alive as the X11 selection owner, so TryClipboardTool
    //    will time out and kill it — clipboard content won't persist after 2 s.
    //    Only useful when xsel is unavailable.
    {
        const char* argv[] = { "xclip", "-selection", "clipboard", nullptr };
        if (TryClipboardTool(argv, data, len)) return true;
    }

    return false;
}


// Runs argv[0], captures its stdout into 'output'.
// Uses poll() so a misbehaving tool can't hang the caller forever.
static bool RunClipboardRead(const char* const* argv,
                              std::string&       output,
                              int                timeoutMs = 2000)
{
    // O_CLOEXEC: both ends auto-close at execvp in the child,
    // preventing them from leaking into the clipboard tool.
    int pfd[2];
    if (pipe2(pfd, O_CLOEXEC) != 0)
        return false;

    pid_t pid = fork();
    if (pid < 0) {
        close(pfd[0]);
        close(pfd[1]);
        return false;
    }

    if (pid == 0) {
        // ---- child -------------------------------------------------------

        // Wire the write end to stdout.
        // dup2 strips O_CLOEXEC from the new fd, so STDOUT_FILENO survives exec.
        close(pfd[0]);
        if (dup2(pfd[1], STDOUT_FILENO) < 0) _exit(1);
        close(pfd[1]);

        // Silence stderr; O_CLOEXEC here, dup2 keeps STDERR_FILENO open.
        int dn = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (dn >= 0) { dup2(dn, STDERR_FILENO); close(dn); }

        // Sweep all other inherited fds — third-party libs may have opened
        // sockets/files without O_CLOEXEC; they must not reach the tool.
        CloseFdsFrom(STDERR_FILENO + 1);

        execvp(argv[0], const_cast<char* const*>(argv));
        _exit(127); // tool not found
    }

    // ---- parent ----------------------------------------------------------

    // CRITICAL: close our copy of the write end first.
    // If we keep it open, read() will never see EOF even after the child exits
    // because the pipe still has a live writer (us).
    close(pfd[1]);

    // Read with a poll() timeout so a hanging tool doesn't freeze the caller.
    output.clear();
    {
        char buf[4096];
        int remaining = timeoutMs;

        while (remaining > 0) {
            struct pollfd pf = { pfd[0], POLLIN, 0 };
            int ret = poll(&pf, 1, remaining);

            if (ret < 0 && errno == EINTR) continue; // interrupted, retry
            if (ret <= 0) break;                     // timeout or error

            ssize_t n = read(pfd[0], buf, sizeof(buf));
            if (n <= 0) break;                       // EOF (child exited) or error
            output.append(buf, static_cast<size_t>(n));

            remaining = timeoutMs; // reset timeout on each successful read
        }
    }
    close(pfd[0]);

    // The child should have exited already (EOF above means it closed stdout).
    // Poll briefly as a safety net, then kill if it somehow lingered.
    const struct timespec kStep = { 0, 50'000'000L }; // 50 ms
    for (int i = 0; i < 40; ++i) {
        int   status = 0;
        pid_t r      = waitpid(pid, &status, WNOHANG);

        if (r == pid)
            return WIFEXITED(status) && WEXITSTATUS(status) == 0;

        if (r < 0 && errno != EINTR) break;
        nanosleep(&kStep, nullptr);
    }

    kill(pid, SIGKILL);
    waitpid(pid, nullptr, 0);
    return false;
}

String CConsole::PasteTextFromClipboard()
{
    // 1. Wayland (Ubuntu 24.04 default).
    //    --no-newline: wl-paste appends \n by default; strip it so callers
    //    get exactly what was copied.
    {
        const char* argv[] = { "wl-paste", "--no-newline", nullptr };
        std::string out;
        if (RunClipboardRead(argv, out)) return String(out);
    }

    // 2. X11 / XWayland — xsel exits immediately after printing.
    {
        const char* argv[] = { "xsel", "--clipboard", "--output", nullptr };
        std::string out;
        if (RunClipboardRead(argv, out)) return String(out);
    }

    // 3. X11 last resort — xclip also exits immediately when reading.
    {
        const char* argv[] = { "xclip", "-selection", "clipboard", "-o", nullptr };
        std::string out;
        if (RunClipboardRead(argv, out)) return String(out);
    }

    return String();
}

// ---------------------------------------------------------
// CConsoleDrawAdapter — double-buffered ANSI rendering
// ---------------------------------------------------------

void CConsoleDrawAdapter::StartDraw(Size size, CConsole* console)
{
    m_console = console;

    if (m_size.width != size.width || m_size.height != size.height)
    {
        m_size = size;
        m_backBuffer.resize(size.width * size.height);
        m_frontBuffer.resize(size.width * size.height);
        m_fullRedraw = true;
    }

    TerminalCell defaultCell;
    std::fill(m_backBuffer.begin(), m_backBuffer.end(), defaultCell);
}

std::string CConsoleDrawAdapter::ExtractUtf8Char(const std::string& str, size_t& i)
{
    if (i >= str.size())
        return "";
    unsigned char c = (unsigned char)str[i];
    int len = 1;
    if      ((c & 0xE0) == 0xC0) len = 2;
    else if ((c & 0xF0) == 0xE0) len = 3;
    else if ((c & 0xF8) == 0xF0) len = 4;

    std::string res = str.substr(i, len);
    i += len;
    return res;
}

void CConsoleDrawAdapter::AppendMoveCursor(std::string& out, int x, int y)
{
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "\x1B[%d;%dH", y + 1, x + 1);
    out.append(buf, len);
}

void CConsoleDrawAdapter::AppendAnsiColor(std::string& out, Color fg, Color bg)
{
    char buf[64];
    int len = snprintf(buf, sizeof(buf),
        "\x1B[38;2;%d;%d;%dm\x1B[48;2;%d;%d;%dm",
        fg.Red(), fg.Green(), fg.Blue(), bg.Red(), bg.Green(), bg.Blue());
    out.append(buf, len);
}

void CConsoleDrawAdapter::PaintCell(int x, int y, const std::string& ch, Color fg, Color bg)
{
    if (x < 0 || x >= m_size.width || y < 0 || y >= m_size.height)
        return;
    int idx = y * m_size.width + x;
    m_backBuffer[idx].character = ch;
    m_backBuffer[idx].fgColor   = fg;
    m_backBuffer[idx].bgColor   = bg;
}

int CConsoleDrawAdapter::PaintText(const Point& position,
    Color textColor,
    Color textBgColor,
    const String& text,
    String hotkeySymbol,
    Color highlightTextColor,
    Color highlightTextBgColor)
{
    int currentX = position.x;
    int currentY = position.y;
    size_t i = 0;
    const std::string& nativeStr = text.native;

    bool prevWasHotkey = false;
    while (i < nativeStr.size())
    {
        if (currentX >= m_size.width) break;
        if (currentY >= m_size.height) break;

        bool isHotkey = (!hotkeySymbol.native.empty() &&
            nativeStr.compare(i, hotkeySymbol.native.size(), hotkeySymbol.native) == 0);
        std::string charStr = ExtractUtf8Char(nativeStr, i);
        if (!isHotkey) 
        {
            if (currentX >= 0 && currentY >= 0)
            {
                int idx = currentY * m_size.width + currentX;
                m_backBuffer[idx].character = charStr;
                m_backBuffer[idx].fgColor   = prevWasHotkey ? highlightTextColor : textColor;
                m_backBuffer[idx].bgColor   = prevWasHotkey ? highlightTextBgColor : textBgColor;
            }
            currentX++;
        }
        prevWasHotkey = isHotkey;
    }
    return currentX - position.x;
}

void CConsoleDrawAdapter::PaintRect(const Rect& rect, Color background, bool keepText)
{
    for (int y = rect.position.y; y < rect.position.y + rect.size.height; ++y)
    {
        if (y < 0 || y >= m_size.height) continue;
        for (int x = rect.position.x; x < rect.position.x + rect.size.width; ++x)
        {
            if (x < 0 || x >= m_size.width) continue;
            int idx = y * m_size.width + x;
            m_backBuffer[idx].bgColor = background;
            if (!keepText)
            {
                m_backBuffer[idx].character = " ";
                m_backBuffer[idx].fgColor   = {200, 200, 200};
            }
        }
    }
}

void CConsoleDrawAdapter::PaintBorder(const Rect& rect,
    Color textColor,
    Color textBgColor,
    BorderStyle style)
{
    const char** syms = (style == BorderStyle::Thin) ? g_boxThin : g_boxThick;

    if (rect.size.height <= 0 || rect.size.width <= 0)
        return;

    int x0  = rect.position.x;
    int y0  = rect.position.y;
    int x1  = x0 + rect.size.width  - 1;
    int y1  = y0 + rect.size.height - 1;

    // Top row
    if (rect.size.width == 1)
        PaintCell(x0, y0, syms[3], textColor, textBgColor); // put | vert border not |-
    else
        PaintCell(x0, y0, syms[0], textColor, textBgColor);
    
    for (int x = x0 + 1; x < x1; ++x)
    {
        PaintCell(x, y0, syms[1], textColor, textBgColor);
    }

    if (rect.size.width > 1)
        PaintCell(x1, y0, syms[2], textColor, textBgColor);

    // Side columns
    for (int y = y0 + 1; y < y1; ++y)
    {
        PaintCell(x0, y, syms[3], textColor, textBgColor);
        if (rect.size.width > 1)
            PaintCell(x1, y, syms[3], textColor, textBgColor);
    }

    // Bottom row
    if (rect.size.height > 1)
    {
        if (rect.size.width == 1)
            PaintCell(x0, y1, syms[3], textColor, textBgColor);
        else
            PaintCell(x0, y1, syms[4], textColor, textBgColor);

        for (int x = x0 + 1; x < x1; ++x)
            PaintCell(x, y1, syms[5], textColor, textBgColor);
        if (rect.size.width > 1)
            PaintCell(x1, y1, syms[6], textColor, textBgColor);
    }
}

void CConsoleDrawAdapter::PaintMenuSeparator(const Point& position,
    int width,
    Color textColor,
    Color textBgColor,
    BorderStyle style)
{
    if (width <= 0)
        return;
    const char** syms = (style == BorderStyle::Thin) ? g_sepThin : g_sepThick;

    PaintCell(position.x, position.y, syms[1], textColor, textBgColor);
    for (int x = position.x + 1; x < position.x + width - 1; ++x)
        PaintCell(x, position.y, syms[0], textColor, textBgColor);
    if (width > 1)
        PaintCell(position.x + width - 1, position.y, syms[2], textColor, textBgColor);
}

void CConsoleDrawAdapter::PaintScrollMark(const Point& position,
    ScrollMarkType type,
    Color textColor,
    Color textBgColor)
{
    const char* ch = (type == ScrollMarkType::Left) ? "<" : ">";
    PaintCell(position.x, position.y, ch, textColor, textBgColor);
}

void CConsoleDrawAdapter::FinishDraw()
{
    std::string out;
    out.reserve(m_size.width * m_size.height * 8);

    int   cursorX    = -1;
    int   cursorY    = -1;
    Color currentFg  = {0, 0, 1};  // impossible value to force first colour write
    Color currentBg  = {0, 0, 1};

    for (int y = 0; y < m_size.height; ++y)
    {
        for (int x = 0; x < m_size.width; ++x)
        {
            int idx = y * m_size.width + x;

            if (!m_fullRedraw && m_backBuffer[idx] == m_frontBuffer[idx])
                continue;

            m_frontBuffer[idx] = m_backBuffer[idx];
            const TerminalCell& cell = m_frontBuffer[idx];

            if (cursorX != x || cursorY != y)
            {
                AppendMoveCursor(out, x, y);
                cursorX = x;
                cursorY = y;
            }

            if (!(currentFg == cell.fgColor) || !(currentBg == cell.bgColor))
            {
                AppendAnsiColor(out, cell.fgColor, cell.bgColor);
                currentFg = cell.fgColor;
                currentBg = cell.bgColor;
            }

            out += cell.character;
            ++cursorX;
        }
    }

    if (!out.empty())
        WriteToStdout(out.data(), out.size());

    m_fullRedraw = false;
}

void CConsoleDrawAdapter::CopyRectWindow(const Rect& rect,
    const Point& targetPosition,
    CConsoleDrawAdapter& consoleOut) const
{
    for (int y = 0; y < rect.size.height; ++y)
    {
        for (int x = 0; x < rect.size.width; ++x)
        {
            int srcX = rect.position.x + x;
            int srcY = rect.position.y + y;
            int dstX = targetPosition.x + x;
            int dstY = targetPosition.y + y;

            if (srcX < 0 || srcX >= m_size.width  || srcY < 0 || srcY >= m_size.height)  continue;
            if (dstX < 0 || dstX >= consoleOut.m_size.width || dstY < 0 || dstY >= consoleOut.m_size.height) continue;

            consoleOut.m_backBuffer[dstY * consoleOut.m_size.width + dstX] =
                m_backBuffer[srcY * m_size.width + srcX];
        }
    }
}

// ---------------------------------------------------------
// CConsoleStateSaver
// ---------------------------------------------------------

CConsoleStateSaver::CConsoleStateSaver()
{
    tcgetattr(STDIN_FILENO, &m_originalTermios);
}

CConsoleStateSaver::~CConsoleStateSaver()
{
    // Restore terminal settings, leave alternate buffer, show cursor
    tcsetattr(STDIN_FILENO, TCSANOW, &m_originalTermios);
    const char* seq = "\x1B[?1049l\x1B[?25h";
    WriteToStdout(seq, strlen(seq));
}

// ---------------------------------------------------------
// GetPanelBorderSymbols
// ---------------------------------------------------------
PanelBorderSymbols GetPanelBorderSymbols()
{
    PanelBorderSymbols symbols;
    symbols.vertical     = g_boxThin[3];
    symbols.horizontal   = g_boxThin[1];
    symbols.left_top     = g_boxThin[0];
    symbols.right_top    = g_boxThin[2];
    symbols.left_bottom  = g_boxThin[4];
    symbols.right_bottom = g_boxThin[6];
    return symbols;
}

}
