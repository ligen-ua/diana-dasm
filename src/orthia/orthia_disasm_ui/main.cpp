#include <memory>  
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include <iostream>

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);


class CMainWindow:public oui::Fullscreen<oui::SimpleBrush<oui::CWindow>>
{
public:
    void ConstuctChilds() override
    {

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
        rootWindow->SetForegroundColor(oui::ColorGray());
        app.Loop(rootWindow);
    }
    catch (const std::exception& err)
    {
        std::cerr << "Error: " << err.what() << "\n";
    }
    return 0;
}
