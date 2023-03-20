#include <memory>  
#include "orthia_text_manager.h"
#include "orthia_model.h"
#include "oui_app.h"
#include <iostream>
#include "oui_menu.h"

orthia::intrusive_ptr<orthia::CTextManager> g_textManager;

void InitLanguage_EN(orthia::intrusive_ptr<orthia::CTextManager> textManager);


class CMainWindow:public oui::SimpleBrush<oui::Fullscreen<oui::CWindow>>
{
    std::shared_ptr<oui::CMenuWindow> m_menu;
public:
    void ConstuctChilds() override
    {
        auto uiMenuTextNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu"));

        m_menu = AddChild_t(std::make_shared<oui::CMenuWindow>());
        m_menu->SetBackgroundColor(oui::ColorBlack());
        
        {
            std::vector<oui::PopupItem> file =
            {
                {
                    ORTHIA_TCSTR("Open &Executable"),
                    []() {}
                },
                {
                    ORTHIA_TCSTR("Open &Process"),
                    []() {}
                },
                {
                    orthia::PlatformString_type(),
                    nullptr
                },
                {
                    ORTHIA_TCSTR("E&xit"),
                    [&]()
                    {
                        if (auto pool = this->m_pool.lock())
                        {
                            pool->ExitLoop();
                        }
                    }
                }
            };
            m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("file")),
                std::move(file)
            );
        }


        {
            std::vector<oui::PopupItem> view =
            {
                {
                    L"Test5",
                    []() {}
                }
            };
            m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("view")),
                std::move(view)
            );
        }
        {
            std::vector<oui::PopupItem> help =
            {
                {
                    L"&Help",
                    []() {}
                },
                {
                    L"&About",
                    []() {}
                }
            };
            m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("help")),
                std::move(help)
            );
        }
        SetOnResize([&]() { 
            
            m_menu->Dock();
        });
    }

    bool ProcessEvent(oui::InputEvent& evt) override
    {
        auto pool = GetPool();
        if (!pool)
        {
            return false;
        }
        oui::Fullscreen<oui::CWindow>::ProcessEvent(evt);
        if (evt.keyEvent.valid)
        {            
            // no focused or focused couldn't process this, check hotkeys
            bool activate = false;
            bool justAlt = (evt.keyState.state & evt.keyState.AnyAlt) &&
                           !(evt.keyState.state & evt.keyState.AnyCtrl) &&
                           !(evt.keyState.state & evt.keyState.AnyShift);
            bool noModifiers = !(evt.keyState.state & evt.keyState.AnyAlt & evt.keyState.AnyCtrl & evt.keyState.AnyShift);
            bool openPopup = false;
            if (evt.keyEvent.rawText.native.empty())
            {
                switch (evt.keyEvent.virtualKey)
                {
                case oui::VirtualKey::None:
                    // just alt, set focus to menu
                    activate = justAlt;
                    break;
                case oui::VirtualKey::kF10:
                    activate = noModifiers;
                    openPopup = true;
                    break;
                }
                
                if (activate)
                {
                    if (m_menu->IsActive())
                    {
                        m_menu->Deactivate();
                        return true;
                    }
                    m_menu->SetPrevFocus(pool->GetFocus());
                    m_menu->Activate();
                    pool->SetFocus(m_menu);
                    if (openPopup)
                    {
                        m_menu->OpenPopup();
                    }
                    return true;
                }
            }


            if (auto focused = pool->GetFocus())
            {
                if (focused->ProcessEvent(evt))
                {
                    return true;
                }
            }
        }
        return true;
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
