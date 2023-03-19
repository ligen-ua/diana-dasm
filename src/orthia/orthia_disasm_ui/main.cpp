#include <memory>  
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include <iostream>
#include "oui_menu.h"

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);


class CMainWindow:public oui::Fullscreen<oui::CWindow>
{
    std::shared_ptr<oui::CMenuWindow> m_menu;
public:
    void ConstuctChilds() override
    {
        auto uiMenuTextNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu"));

        m_menu = AddChild_t(std::make_shared<oui::CMenuWindow>());
        m_menu->SetBackgroundColor(oui::ColorBlack());
        

        m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("file")), []() {});
        m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("workspace")), []() {});
        m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("view")), []() {});
        m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("help")), []() {});


        SetOnResize([&]() { 
            
            m_menu->Dock();
        });
    }
};

int main(int argc, const char* argv[])
{
    std::cout << "Welcome to Orthia Disasm\n\n";

    try
    {
        orthia::CProgramModel programModel;
        std::cout.flush();

        g_textManager = new orthia::CTextManager();
        InitLanguage_EN(g_textManager);

        oui::CConsoleApp app;

        auto rootWindow = std::make_shared<CMainWindow>();
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
    }
    return 0;
}
