#pragma once
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include "oui_menu.h"
#include "oui_hotkey.h"


extern orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

class CMainWindow:public oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>
{
    std::shared_ptr<oui::CMenuWindow> m_menu;
    oui::CHotkeyStorage m_hotkeys;

    void ConstuctMenu();
    void ToggleMenu(bool openPopup);

public:
    void ConstuctChilds() override;
    bool ProcessEvent(oui::InputEvent& evt) override;
};