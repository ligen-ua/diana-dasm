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
#include "oui_open_process_dialog.h"
#include "oui_shellcode_dialog.h"
#include "ui_workspace_window.h"
#include "ui_modules_window.h"
#include "ui_command_window.h"
#include "ui_sections_window.h"

extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

struct InitialOpenFileInfo
{
    int errorCode = 0;
    oui::String name;
    std::shared_ptr<oui::IFile2> file;
    std::shared_ptr<oui::IProcess> process;

};

class CMainWindow:public oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>, public orthia::IUIEventHandler
{
    std::shared_ptr<orthia::CProgramModel> m_model;

    std::shared_ptr<oui::CMenuWindow> m_menu;
    std::shared_ptr<oui::CPanelContainerWindow> m_panelContainerWindow;
    std::shared_ptr<CDisasmWindow> m_disasmWindow;
    std::shared_ptr<COutputWindow> m_outputWindow;
    std::shared_ptr<CWorkspaceWindow> m_workspaceWindow;
    std::shared_ptr<CModulesWindow> m_modulesWindow;
    std::shared_ptr<CCommandWindow> m_commandWindow;
    std::shared_ptr<CSectionsWindow> m_sectionsWindow;
    std::shared_ptr<oui::CPanelGroupWindow> m_defaultGroup;

    oui::CHotkeyStorage m_hotkeys;
    std::vector<InitialOpenFileInfo> m_fileToOpen;
    std::vector<oui::String> m_initialText;

    CUIStateManager m_stateManager;

    std::function<void()> m_workspaceInitializer;
    void ConstuctMenu();
    void ToggleMenu(bool openPopup);
    void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
    void OpenExecutable();
    void OpenProcess();
    void CloseCurrentItem();
    void ShowAbout();
    void ShowHelp();
    void ToggleWorkspaceView();
    void OpenHistoryModal();

    oui::fsui::OpenResult HandleOpenExecutable(std::shared_ptr<oui::COpenFileDialog> dialog,
        std::shared_ptr<oui::IFile2> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
        int fileType);

    oui::fsui::OpenResult HandleOpenProcess(std::shared_ptr<oui::COpenProcessDialog> dialog,
        std::shared_ptr<oui::IProcess> process,
        oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler);

    void OnWorkspaceItemChanged(int itemId) override;
    void OnPreWorkspaceItemChange(int itemId) override;
    void OnWorkspaceDataRefreshed(int itemId) override;
    void OnWorkspaceItemRemoved(int itemId) override;

    void OnWorkspaceItemChanged(const oui::fsui::OpenResult& result);
    void OnFileOpen(std::shared_ptr<oui::IFile> file, const oui::fsui::OpenResult& result);
    void ShowShellcodeDialog(std::shared_ptr<oui::IFile2> file,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler = nullptr);
    bool AsyncOpenFileAsShellcode(std::shared_ptr<oui::IFile2> file, DI_UINT64 baseAddress, int dianaMode,
        oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> outerHandler = nullptr);
    void SetFocusImpl() override;

    void SetDefaultTitle();
public:
    CMainWindow(std::shared_ptr<orthia::CProgramModel> model);
    void AddInitialArgument(const InitialOpenFileInfo& info);
    void AddInitialTextOutputInfo(const oui::String& text);
    void ConstructChilds() override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
    bool AsyncOpenFile(std::shared_ptr<oui::IFile2> file);
    bool AsyncOpenProcess(std::shared_ptr<oui::IProcess> process);
    std::shared_ptr<CSectionsWindow> GetSectionsWindow() { return m_sectionsWindow; }
    void SwitchToSectionsPanel();
};