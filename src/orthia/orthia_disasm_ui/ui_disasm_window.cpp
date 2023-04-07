#include "ui_disasm_window.h"

CDisasmWindow::CDisasmWindow(std::function<oui::String()> getCaption)
    : 
        oui::CPanelWindow(getCaption)
{
}
