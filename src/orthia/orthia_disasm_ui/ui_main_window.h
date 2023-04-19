#pragma once
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include "oui_menu.h"
#include "oui_hotkey.h"
#include "oui_containers.h"
#include "ui_disasm_window.h"
#include "ui_output_window.h"


extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

class CMainWindow:public oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>
{
    std::shared_ptr<oui::CMenuWindow> m_menu;
    std::shared_ptr<oui::CPanelContainerWindow> m_panelContainerWindow;
    std::shared_ptr<CDisasmWindow> m_disasmWindow;
    std::shared_ptr<COutputWindow> m_outputWindow;

    oui::CHotkeyStorage m_hotkeys;

    void ConstuctMenu();
    void ToggleMenu(bool openPopup);
    void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
    void OpenExecutable();

public:
    void ConstuctChilds() override;
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
};