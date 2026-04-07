#include "oui_input_posix.h"
#include "oui_input.h"
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <string.h>

namespace oui
{

// Pipe used to wake up the blocking select() from Interrupt()
struct CConsoleInputReader::Impl
{
    int pipeFd[2] = {-1, -1};

    Impl()
    {
        pipe(pipeFd);
    }
    ~Impl()
    {
        if (pipeFd[0] >= 0) close(pipeFd[0]);
        if (pipeFd[1] >= 0) close(pipeFd[1]);
    }
};

CConsoleInputReader::CConsoleInputReader()
    : m_impl(new Impl())
{
}

CConsoleInputReader::~CConsoleInputReader()
{
}

void CConsoleInputReader::Interrupt()
{
    char c = 1;
    write(m_impl->pipeFd[1], &c, 1);
}

// Map ANSI escape sequences to VirtualKey
static VirtualKey MapEscapeSequence(const char* buf, int len, KeyState& keyState)
{
    if (len < 2) return VirtualKey::None;

    // CSI sequences: ESC [ ...
    if (buf[1] == '[' && len >= 3)
    {
        // ESC [ A/B/C/D — cursor keys
        if (len == 3)
        {
            switch (buf[2])
            {
            case 'A': return VirtualKey::Up;
            case 'B': return VirtualKey::Down;
            case 'C': return VirtualKey::Right;
            case 'D': return VirtualKey::Left;
            case 'H': return VirtualKey::Home;
            case 'F': return VirtualKey::End;
            case 'Z': keyState.state |= KeyState::AnyShift; return VirtualKey::Tab;
            }
        }
        // ESC [ 1 ; mod X — modified cursor keys
        if (len == 6 && buf[2] == '1' && buf[3] == ';' && buf[5] >= 'A' && buf[5] <= 'D')
        {
            int mod = buf[4] - '1';
            if (mod & 1) keyState.state |= KeyState::AnyShift | KeyState::LeftShift | KeyState::RightShift;
            if (mod & 2) keyState.state |= KeyState::AnyAlt | KeyState::LeftAlt | KeyState::RightAlt;
            if (mod & 4) keyState.state |= KeyState::AnyCtrl | KeyState::LeftCtrl | KeyState::RightCtrl;
            switch (buf[5])
            {
            case 'A': return VirtualKey::Up;
            case 'B': return VirtualKey::Down;
            case 'C': return VirtualKey::Right;
            case 'D': return VirtualKey::Left;
            }
        }
        // ESC [ N ~ — special keys
        if (buf[len - 1] == '~')
        {
            // parse number
            int n = 0;
            for (int i = 2; i < len - 1 && buf[i] != ';'; ++i)
                n = n * 10 + (buf[i] - '0');
            switch (n)
            {
            case 1:  return VirtualKey::Home;
            case 2:  return VirtualKey::Insert;
            case 3:  return VirtualKey::Delete;
            case 4:  return VirtualKey::End;
            case 5:  return VirtualKey::PageUp;
            case 6:  return VirtualKey::PageDown;
            case 7:  return VirtualKey::Home;
            case 8:  return VirtualKey::End;
            case 11: return VirtualKey::kF1;
            case 12: return VirtualKey::kF2;
            case 13: return VirtualKey::kF3;
            case 14: return VirtualKey::kF4;
            case 15: return VirtualKey::kF5;
            case 17: return VirtualKey::kF6;
            case 18: return VirtualKey::kF7;
            case 19: return VirtualKey::kF8;
            case 20: return VirtualKey::kF9;
            case 21: return VirtualKey::kF10;
            case 23: return VirtualKey::kF11;
            case 24: return VirtualKey::kF12;
            }
        }
    }
    // SS3 sequences: ESC O ...
    if (buf[1] == 'O' && len == 3)
    {
        switch (buf[2])
        {
        case 'P': return VirtualKey::kF1;
        case 'Q': return VirtualKey::kF2;
        case 'R': return VirtualKey::kF3;
        case 'S': return VirtualKey::kF4;
        case 'H': return VirtualKey::Home;
        case 'F': return VirtualKey::End;
        case 'A': return VirtualKey::Up;
        case 'B': return VirtualKey::Down;
        case 'C': return VirtualKey::Right;
        case 'D': return VirtualKey::Left;
        }
    }
    return VirtualKey::None;
}

// Map a control character (0x01-0x1A) to VirtualKey
static VirtualKey ControlCharToVKey(char c, KeyState& keyState)
{
    if (c >= 1 && c <= 26)
    {
        keyState.state |= KeyState::AnyCtrl | KeyState::LeftCtrl;
        // map to letter keys: Ctrl+A=1, ..., Ctrl+Z=26
        return static_cast<VirtualKey>(static_cast<int>(VirtualKey::kA) + (c - 1));
    }
    return VirtualKey::None;
}

bool CConsoleInputReader::Read(std::vector<InputEvent>& input)
{
    input.clear();

    // Wait for input on stdin or the interrupt pipe
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(m_impl->pipeFd[0], &fds);
    int maxFd = m_impl->pipeFd[0] + 1;

    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 100000; // 100 ms

    int ret = select(maxFd, &fds, nullptr, nullptr, &tv);
    if (ret < 0)
    {
        if (errno == EINTR)
            return true;
        return false;
    }

    // Drain the interrupt pipe
    if (FD_ISSET(m_impl->pipeFd[0], &fds))
    {
        char tmp[64];
        read(m_impl->pipeFd[0], tmp, sizeof(tmp));
        return true;
    }

    if (!FD_ISSET(STDIN_FILENO, &fds))
    {
        // Timeout — return empty (gives caller a chance to redraw)
        return true;
    }

    // Check for SIGWINCH resize
    {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0)
        {
            // We'll let the app detect size changes on every iteration via GetSize().
            // Here we just deliver a resize event when we have no buffered input yet.
        }
    }

    char buf[64];
    int n = (int)read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0)
    {
        return n == 0; // EOF = false (exit), EINTR-style = true
    }

    int i = 0;
    while (i < n)
    {
        InputEvent evt;
        unsigned char c = (unsigned char)buf[i];

        if (c == 0x1B) // ESC or escape sequence
        {
            // Check if more bytes follow (escape sequence) or standalone ESC
            if (i + 1 < n)
            {
                // Try to consume an escape sequence
                int seqLen = 1;
                // SS3: ESC O x  (3 bytes)
                // CSI: ESC [ ... final_byte
                char seq[32];
                seq[0] = 0x1B;
                int j = i + 1;
                int maxSeq = (n - i) < 20 ? (n - i) : 20;
                while (seqLen < maxSeq)
                {
                    seq[seqLen] = buf[j];
                    seqLen++;
                    j++;
                    char last = buf[j - 1];
                    // CSI sequence ends at a letter or ~
                    if (seqLen >= 3)
                    {
                        if (seq[1] == '[' || seq[1] == 'O')
                        {
                            if ((last >= 'A' && last <= 'Z') ||
                                (last >= 'a' && last <= 'z') ||
                                last == '~')
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                seq[seqLen] = '\0';

                KeyState keyState;
                VirtualKey vk = MapEscapeSequence(seq, seqLen, keyState);
                if (vk != VirtualKey::None)
                {
                    evt.keyEvent.valid = true;
                    evt.keyEvent.virtualKey = vk;
                    evt.keyState = keyState;
                    i += seqLen;
                }
                else
                {
                    // Alt + next character
                    i++;
                    if (i < n)
                    {
                        evt.keyEvent.valid = true;
                        evt.keyEvent.rawText.native = std::string(1, buf[i]);
                        evt.keyState.state |= KeyState::AnyAlt | KeyState::LeftAlt;
                        i++;
                    }
                    else
                    {
                        // Standalone ESC
                        evt.keyEvent.valid = true;
                        evt.keyEvent.virtualKey = VirtualKey::Escape;
                    }
                }
            }
            else
            {
                // Standalone ESC
                evt.keyEvent.valid = true;
                evt.keyEvent.virtualKey = VirtualKey::Escape;
                i++;
            }
        }
        else if (c == '\r' || c == '\n')
        {
            evt.keyEvent.valid = true;
            evt.keyEvent.virtualKey = VirtualKey::Enter;
            i++;
        }
        else if (c == '\t')
        {
            evt.keyEvent.valid = true;
            evt.keyEvent.virtualKey = VirtualKey::Tab;
            i++;
        }
        else if (c == 0x7F || c == '\b')
        {
            evt.keyEvent.valid = true;
            evt.keyEvent.virtualKey = VirtualKey::Backspace;
            i++;
        }
        else if (c < 0x20) // control characters
        {
            evt.keyEvent.valid = true;
            evt.keyEvent.virtualKey = ControlCharToVKey((char)c, evt.keyState);
            i++;
        }
        else
        {
            // Regular UTF-8 text — grab all bytes of this codepoint
            int cpLen = 1;
            if      ((c & 0xE0) == 0xC0) cpLen = 2;
            else if ((c & 0xF0) == 0xE0) cpLen = 3;
            else if ((c & 0xF8) == 0xF0) cpLen = 4;

            int avail = n - i;
            if (cpLen > avail) cpLen = avail;

            evt.keyEvent.valid = true;
            evt.keyEvent.rawText.native.assign(buf + i, cpLen);
            i += cpLen;
        }

        if (evt.keyEvent.valid || evt.mouseEvent.valid || evt.resizeEvent.valid)
        {
            // Map single ASCII letter/digit to a VirtualKey as well
            if (evt.keyEvent.rawText.native.size() == 1)
            {
                char ch = evt.keyEvent.rawText.native[0];
                if (ch >= 'a' && ch <= 'z') 
                    evt.keyEvent.virtualKey = static_cast<VirtualKey>(static_cast<int>(VirtualKey::kA) + (ch - 'a'));
                else if (ch >= 'A' && ch <= 'Z')
                    evt.keyEvent.virtualKey = static_cast<VirtualKey>(static_cast<int>(VirtualKey::kA) + (ch - 'A'));
                else if (ch >= '0' && ch <= '9')
                    evt.keyEvent.virtualKey = static_cast<VirtualKey>(static_cast<int>(VirtualKey::k0) + (ch - '0'));
            }
            //printf("DEBUG: %s -> %i\n", evt.keyEvent.rawText.native.c_str(), (int)evt.keyEvent.virtualKey);
            input.push_back(evt);
        }
    }

    return true;
}

} // namespace oui

