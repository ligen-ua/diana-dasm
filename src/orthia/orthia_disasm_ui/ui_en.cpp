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
        << textManager->RegisterValue(ORTHIA_TCSTR("open_saved"), ORTHIA_TCSTR("Open &Saved Process"))
        << textManager->RegisterValue(ORTHIA_TCSTR("close_item"), ORTHIA_TCSTR("&Close"))
        << textManager->RegisterValue(ORTHIA_TCSTR("exit"), ORTHIA_TCSTR("E&xit"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.menu.view"))
        << textManager->RegisterValue(ORTHIA_TCSTR("workspace"), ORTHIA_TCSTR("&Workspace"))
        << textManager->RegisterValue(ORTHIA_TCSTR("history"), ORTHIA_TCSTR("&History"))
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
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("&Disassembly"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.disasm.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy"), ORTHIA_TCSTR("&Copy"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy_assembler"), ORTHIA_TCSTR("Copy &assembler"))
        << textManager->RegisterValue(ORTHIA_TCSTR("show_references"), ORTHIA_TCSTR("Show &references"))
        ;

    // modules
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.modules"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("&Modules"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.modules.columns"))
        << textManager->RegisterValue(ORTHIA_TCSTR("name"), ORTHIA_TCSTR("Name"))
        << textManager->RegisterValue(ORTHIA_TCSTR("address"), ORTHIA_TCSTR("Address"))
        << textManager->RegisterValue(ORTHIA_TCSTR("mapped-size"), ORTHIA_TCSTR("Size"))
        << textManager->RegisterValue(ORTHIA_TCSTR("status"), ORTHIA_TCSTR("Status"))
        << textManager->RegisterValue(ORTHIA_TCSTR("full-path"), ORTHIA_TCSTR("Path"))
        ;
    // names
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.names"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Names"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.names.columns"))
        << textManager->RegisterValue(ORTHIA_TCSTR("name"), ORTHIA_TCSTR("Name"))
        << textManager->RegisterValue(ORTHIA_TCSTR("address"), ORTHIA_TCSTR("Address"))
        << textManager->RegisterValue(ORTHIA_TCSTR("type"), ORTHIA_TCSTR("Type"))
        ;
    // output
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.output"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("&Output"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.output.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy"), ORTHIA_TCSTR("&Copy"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.commands"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("&Commands"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.commands.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy"), ORTHIA_TCSTR("&Copy"))
        ;

    // sections
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.sections"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("&Sections"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.sections.columns"))
        << textManager->RegisterValue(ORTHIA_TCSTR("name"),    ORTHIA_TCSTR("Name"))
        << textManager->RegisterValue(ORTHIA_TCSTR("address"), ORTHIA_TCSTR("VirtAddr"))
        << textManager->RegisterValue(ORTHIA_TCSTR("size"),    ORTHIA_TCSTR("Size"))
        << textManager->RegisterValue(ORTHIA_TCSTR("flags"),   ORTHIA_TCSTR("RWX"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.sections.attrs"))
        << textManager->RegisterValue(ORTHIA_TCSTR("field"), ORTHIA_TCSTR("Field"))
        << textManager->RegisterValue(ORTHIA_TCSTR("value"), ORTHIA_TCSTR("Value"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.modules.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("show_sections"), ORTHIA_TCSTR("Show &Sections"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.names.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("sort_by_type"),    ORTHIA_TCSTR("Sort by &Type"))
        << textManager->RegisterValue(ORTHIA_TCSTR("sort_by_name"),    ORTHIA_TCSTR("Sort by &Name"))
        << textManager->RegisterValue(ORTHIA_TCSTR("sort_by_address"), ORTHIA_TCSTR("Sort by &Address"))
        ;

    // workspace
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.workspace"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Workspace"))
        ;

    // help
    textManager->RegisterNode(ORTHIA_TCSTR("ui.panels.help.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy"), ORTHIA_TCSTR("&Copy"))
        ;

    // editbox
    textManager->RegisterNode(ORTHIA_TCSTR("ui.editbox.contextmenu"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cut"),   ORTHIA_TCSTR("Cu&t"))
        << textManager->RegisterValue(ORTHIA_TCSTR("copy"),  ORTHIA_TCSTR("&Copy"))
        << textManager->RegisterValue(ORTHIA_TCSTR("paste"), ORTHIA_TCSTR("&Paste"))
        ;

    // dialogs
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.basedialog"))
        << textManager->RegisterValue(ORTHIA_TCSTR("ok"), ORTHIA_TCSTR("OK"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cancel"), ORTHIA_TCSTR("Cancel"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opening"), ORTHIA_TCSTR("Opening: %1"))
        << textManager->RegisterValue(ORTHIA_TCSTR("error"), ORTHIA_TCSTR("Error: %1"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.openfile"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Open Executable"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.openprocess"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Open Process"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.goto"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Goto Address"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.opensaved"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Open Saved Process"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.opensaved.columns"))
        << textManager->RegisterValue(ORTHIA_TCSTR("process"), ORTHIA_TCSTR("Process folder"))
        << textManager->RegisterValue(ORTHIA_TCSTR("modified"), ORTHIA_TCSTR("Modified"))
        << textManager->RegisterValue(ORTHIA_TCSTR("size"), ORTHIA_TCSTR("Size"))
        << textManager->RegisterValue(ORTHIA_TCSTR("notes"), ORTHIA_TCSTR("Notes"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.xrefs"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("References"))
        ;
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.goto.columns"))
        << textManager->RegisterValue(ORTHIA_TCSTR("address"), ORTHIA_TCSTR("Address"))
        << textManager->RegisterValue(ORTHIA_TCSTR("comment"), ORTHIA_TCSTR("Comment"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.editcomment"))
        << textManager->RegisterValue(ORTHIA_TCSTR("label"), ORTHIA_TCSTR("New comment:"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.shellcode"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"),    ORTHIA_TCSTR("Open as Shellcode"))
        << textManager->RegisterValue(ORTHIA_TCSTR("message"),    ORTHIA_TCSTR("File format not recognized. Open as raw shellcode?"))
        << textManager->RegisterValue(ORTHIA_TCSTR("addr-label"), ORTHIA_TCSTR("Load address:"))
        << textManager->RegisterValue(ORTHIA_TCSTR("mode-label"), ORTHIA_TCSTR("Architecture (Left/Right to change):"))
        ;

    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.autotasks"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("New Database"))
        << textManager->RegisterValue(ORTHIA_TCSTR("message"), ORTHIA_TCSTR("%1 has no database yet. Start now:"))
        << textManager->RegisterValue(ORTHIA_TCSTR("analyze"), ORTHIA_TCSTR("Analyze the code"))
        << textManager->RegisterValue(ORTHIA_TCSTR("symbols"), ORTHIA_TCSTR("Load the symbols"))
        << textManager->RegisterValue(ORTHIA_TCSTR("hint"), ORTHIA_TCSTR("Space toggles. Later: .analyze, .reload"))
        << textManager->RegisterValue(ORTHIA_TCSTR("ok"), ORTHIA_TCSTR("OK"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cancel"), ORTHIA_TCSTR("Cancel"))
        ;

    // main
    textManager->RegisterNode(ORTHIA_TCSTR("ui.dialog.main"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption"), ORTHIA_TCSTR("Orthia Disassembler"))
        << textManager->RegisterValue(ORTHIA_TCSTR("caption-file"), ORTHIA_TCSTR("Orthia File: %1"))
        << textManager->RegisterValue(ORTHIA_TCSTR("analyzing-file"), ORTHIA_TCSTR("Analyzing...."))
        << textManager->RegisterValue(ORTHIA_TCSTR("analysis-complete"), ORTHIA_TCSTR("Background analysis complete"))
        << textManager->RegisterValue(ORTHIA_TCSTR("reloading-module"), ORTHIA_TCSTR("Reloading module: \"%1\" ..."))
        << textManager->RegisterValue(ORTHIA_TCSTR("incoming-entries"), ORTHIA_TCSTR("Module \"%1\": %2 entry points from the calls and jumps of other modules"))
        << textManager->RegisterValue(ORTHIA_TCSTR("loading-symbols"), ORTHIA_TCSTR("Loading symbols: \"%1\" ..."))
        << textManager->RegisterValue(ORTHIA_TCSTR("loading-symbols-done"), ORTHIA_TCSTR("Symbol loading complete: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("analyzing-private-symbols"), ORTHIA_TCSTR("Analyzing private symbols: \"%1\" ..."))
        << textManager->RegisterValue(ORTHIA_TCSTR("analyzing-private-symbols-done"), ORTHIA_TCSTR("Private symbols analysis complete: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("analyzing-floating"), ORTHIA_TCSTR("Analyzing floating code: \"%1\" ..."))
        << textManager->RegisterValue(ORTHIA_TCSTR("floating-stale"), ORTHIA_TCSTR("Floating module \"%1\" is stale: its memory changed since it was found (.analyze /f checks it again)"))
        << textManager->RegisterValue(ORTHIA_TCSTR("floating-refreshed"), ORTHIA_TCSTR("Floating module \"%1\" is checked again: the memory as it is now"))
        << textManager->RegisterValue(ORTHIA_TCSTR("floating-gone"), ORTHIA_TCSTR("Floating module \"%1\" is removed: its allocation or its executable entry points are gone"))
        << textManager->RegisterValue(ORTHIA_TCSTR("floating-limit"), ORTHIA_TCSTR("Floating code: the limit of %1 modules is reached, the rest is not analyzed"))
        << textManager->RegisterValue(ORTHIA_TCSTR("symbols-mismatch"), ORTHIA_TCSTR("Symbol file mismatched: \"%1\" ..."))
        << textManager->RegisterValue(ORTHIA_TCSTR("symbols-elf-unsupported"), ORTHIA_TCSTR("Symbols for \"%1\": debug file \"%2\" found, but ELF symbol loading is not supported yet"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opened-file"), ORTHIA_TCSTR("Opened: %1 %2, %3"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opened-layout"), ORTHIA_TCSTR("base %1, entry %2, size %3"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opened-managed"), ORTHIA_TCSTR(".NET, %1"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opened-process"), ORTHIA_TCSTR("Opened: process %1, main module: %2"))
        << textManager->RegisterValue(ORTHIA_TCSTR("module-sha1"), ORTHIA_TCSTR("SHA1: %1"))
        << textManager->RegisterValue(ORTHIA_TCSTR("opening"), ORTHIA_TCSTR("Opening: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("closing"), ORTHIA_TCSTR("Closing: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("readme-header"), ORTHIA_TCSTR("Orthia Disasm Database Folder"))
        << textManager->RegisterValue(ORTHIA_TCSTR("original-name"), ORTHIA_TCSTR("Original Name: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("database-file"), ORTHIA_TCSTR("Database file: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("opening-saved"), ORTHIA_TCSTR("Opening the saved process folder: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("saved-no-modules"), ORTHIA_TCSTR("The folder has no module list (modules.xml): only the floating code has bytes"))
        << textManager->RegisterValue(ORTHIA_TCSTR("saved-name"), ORTHIA_TCSTR("%1 (saved)"))
        << textManager->RegisterValue(ORTHIA_TCSTR("database-reused"), ORTHIA_TCSTR("Using the database of an earlier open of this process (--overwrite-db for a new one)"))
        ;

    // model
    textManager->RegisterNode(ORTHIA_TCSTR("model.errors"))
        << textManager->RegisterValue(ORTHIA_TCSTR("file-error-name"), ORTHIA_TCSTR("Can't open file: \"%1\""))
        << textManager->RegisterValue(ORTHIA_TCSTR("file-error-name-code"), ORTHIA_TCSTR("Can't open file: \"%1\", %2"))
        << textManager->RegisterValue(ORTHIA_TCSTR("unknown"), ORTHIA_TCSTR("Can't parse module file, unknown format"))
        << textManager->RegisterValue(ORTHIA_TCSTR("empty"), ORTHIA_TCSTR("File is empty"))
        << textManager->RegisterValue(ORTHIA_TCSTR("too-big"), ORTHIA_TCSTR("File is too big"))
        << textManager->RegisterValue(ORTHIA_TCSTR("no-app-dir"), ORTHIA_TCSTR("Can't locate application folder"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cant-open-file"), ORTHIA_TCSTR("Can't open file"))
        << textManager->RegisterValue(ORTHIA_TCSTR("invalid-image-base"), ORTHIA_TCSTR("ImageBase is invalid"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cant-create-fs"), ORTHIA_TCSTR("Can't create folder: %1"))
        << textManager->RegisterValue(ORTHIA_TCSTR("cancelled"), ORTHIA_TCSTR("Cancelled by user"))
        ;
}