#include "oui_console.h"
#include "windows.h"

namespace oui
{
    CConsole::CConsole()
    {

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
}