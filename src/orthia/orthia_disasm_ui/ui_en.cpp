#include "orthia_text_manager.h"

void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager)
{
    // menu
    textManager->RegisterNode(ORTHIA_TCSTR("ui.menu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("file"), ORTHIA_TCSTR("&File"))
        << textManager->RegisterValue(ORTHIA_TCSTR("workspace"), ORTHIA_TCSTR("&Workspace"))
        << textManager->RegisterValue(ORTHIA_TCSTR("view"), ORTHIA_TCSTR("&View"))
        << textManager->RegisterValue(ORTHIA_TCSTR("help"), ORTHIA_TCSTR("&Help"))
        ;   
    textManager->RegisterNode(ORTHIA_TCSTR("ui.menu.file"))
        << textManager->RegisterValue(ORTHIA_TCSTR("open_executable"), ORTHIA_TCSTR("Open &Executable"))
        << textManager->RegisterValue(ORTHIA_TCSTR("open_process"), ORTHIA_TCSTR("Open &Process"))
        << textManager->RegisterValue(ORTHIA_TCSTR("exit"), ORTHIA_TCSTR("E&xit"))
        ;

    // modal
    textManager->RegisterNode(ORTHIA_TCSTR("ui.modal.exit"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cancel"), ORTHIA_TCSTR("Cancel"))
        << textManager->RegisterValue(ORTHIA_TCSTR("quit"), ORTHIA_TCSTR("Quit"))
        << textManager->RegisterValue(ORTHIA_TCSTR("message"), ORTHIA_TCSTR("Are you sure you want to exit?"))
        ;

    // panels
    // disasm
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.disasm"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Disassembly"))
        ;

    // output
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.output"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Output"))
        ;

}