#pragma once
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include "oui_menu.h"
#include "oui_hotkey.h"
#include "oui_containers.h"
#include "ui_disasm_window.h"
#include "ui_output_window.h"
#include "oui_open_file_dialog.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

struct InitialOpenFileInfo
{
    int errorCode = 0;
    oui::String name;
    std::shared_ptr<oui::IFile> file;
};

class CMainWindow:public oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>
{
    std::shared_ptr<orthia::CProgramModel> m_model;

    std::shared_ptr<oui::CMenuWindow> m_menu;
    std::shared_ptr<oui::CPanelContainerWindow> m_panelContainerWindow;
    std::shared_ptr<CDisasmWindow> m_disasmWindow;
    std::shared_ptr<COutputWindow> m_outputWindow;

    oui::CHotkeyStorage m_hotkeys;
    std::vector<InitialOpenFileInfo> m_fileToOpen;
    std::vector<oui::String> m_initialText;

    void ConstuctMenu();
    void ToggleMenu(bool openPopup);
    void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
    void OpenExecutable();
    void OpenProcess();

    oui::fsui::OpenResult HandleOpenExecutable(std::shared_ptr<oui::COpenFileDialog> dialog,
        std::shared_ptr<oui::IFile> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler);

    void OnWorkspaceItemChanged();
    void OnFileOpen(std::shared_ptr<oui::IFile> file, const oui::fsui::OpenResult& result);

    void SetDefaultTitle();
public:
    CMainWindow(std::shared_ptr<orthia::CProgramModel> model);
    void AddInitialArgument(const InitialOpenFileInfo& info);
    void AddInitialTextOutputInfo(const oui::String& text);
    void ConstructChilds() override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
    bool AsyncOpenFile(std::shared_ptr<oui::IFile> file);
};