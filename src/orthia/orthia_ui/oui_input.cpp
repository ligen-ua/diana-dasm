#include "oui_input.h"
#include "windows.h"


namespace oui
{

    
    CConsoleInputReader::CConsoleInputReader()
    {
    }
    bool CConsoleInputReader::Read(std::vector<InputEvent>& input)
    {
        Sleep(INFINITE);
        return false;
    }
}
