#include <memory>  
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);

int main(int argc, const char* argv[]) 
{
    orthia::CProgramModel programModel;

    g_textManager = new orthia::CTextManager();
    InitLanguage_EN(g_textManager);

    oui::CConsoleApp app;
    app.Loop(std::make_shared<oui::CWindow>());
    return 0;
}
