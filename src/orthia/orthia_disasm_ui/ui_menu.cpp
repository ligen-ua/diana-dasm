#include "ui_main_window.h"

void CMainWindow::ToggleMenu(bool openPopup)
{
    if (m_menu->IsActive())
    {
        m_menu->Deactivate();
        return;
    }
    m_menu->Activate();
    if (openPopup)
    {
        m_menu->OpenPopup();
    }
}

oui::fsui::OpenResult CMainWindow::HandleOpenExecutable(std::shared_ptr<oui::COpenFileDialog> dialog,
    std::shared_ptr<oui::IFile> file, 
    oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler)
{ 
    if (dialog && file && completeHandler)
    {
        // means open dialog manager to open a file
        // setup UI proxy on success and pass the handler to FS handler
        auto me = oui::GetPtr_t<CMainWindow>(this);
        std::weak_ptr<CMainWindow> weakMe = me;
        if (!me)
        {
            return oui::fsui::OpenResult();
        }
        oui::fsui::FileCompleteHandler_type rawHandler = completeHandler->GetHandler();
        completeHandler->SetHandler(
            [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IFile> file, const oui::fsui::OpenResult& result) {
    
            if (auto p = weakMe.lock())
            {
                if (result.error.native.empty())
                {
                    // finally
                    p->OnWorkspaceItemChanged();
                }
            }
            rawHandler(op, file, result);
        });

        m_model->GetFileSystem()->AsyncExecute(dialog->GetThread(), [file, model = m_model, completeHandler = std::move(completeHandler)] {
            model->AddExecutable(file, completeHandler, true);
        });
    }
    return oui::fsui::OpenResult();
};

void CMainWindow::OpenExecutable()
{
    auto openFileNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.openfile"));

    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return;
    }
    
    // create open dialog
    auto dialog = AddChildAndInit_t(std::make_shared<oui::COpenFileDialog>(oui::String(),
        openFileNode->QueryValue(ORTHIA_TCSTR("opening")),
        openFileNode->QueryValue(ORTHIA_TCSTR("error")),
        [=](std::shared_ptr<oui::COpenFileDialog> dlg, std::shared_ptr<oui::IFile> file, oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> handler) {
            if (auto p = weakMe.lock())
            {
                return p->HandleOpenExecutable(dlg, file, handler);
            }
            oui::fsui::OpenResult result(OUI_TCSTR("Error"));
            return result;
        },
        m_model->GetFileSystem(),
        oui::FileInfo::flag_any_executable));
    dialog->SetCaption(openFileNode->QueryValue(ORTHIA_TCSTR("caption")));
    dialog->Dock();
}
void CMainWindow::ConstuctMenu()
{
    auto uiMenuTextNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu"));
    auto uiMenuTextNodeFile = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu.file"));

    m_menu = AddChild_t(std::make_shared<oui::CMenuWindow>());
    m_menu->SetBackgroundColor(oui::ColorBlack());

    m_hotkeys.Register(oui::Hotkey(oui::KeyState(),
        oui::VirtualKey::kF10), [=]() { ToggleMenu(true);  });

    {
        // build file menu
        std::vector<oui::PopupItem> file =
        {
            {
                uiMenuTextNodeFile->QueryValue(ORTHIA_TCSTR("open_executable")),
                [this]() { OpenExecutable();  },
                oui::Hotkey(oui::VirtualKey::kE)
            },
            {
                uiMenuTextNodeFile->QueryValue(ORTHIA_TCSTR("open_process")),
                []() {},
                oui::Hotkey(oui::VirtualKey::kX)
            },
            {
                orthia::PlatformString_type(),
                nullptr
            },
            {
                uiMenuTextNodeFile->QueryValue(ORTHIA_TCSTR("exit")),
                [&]()
                {
                    if (auto pool = this->m_pool.lock())
                    {
                        pool->ExitLoop();
                    }
                },
                oui::Hotkey(oui::VirtualKey::kX)
            }
        };
        auto button = m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("file")),
            std::move(file)
        );
        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kF), [=]() { m_menu->SelectAndOpenPopup(button, false);  });
    }


    {
        // build view menu
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
                []() {},
                oui::Hotkey(oui::VirtualKey::kH)
            },
            {
                L"&About",
                []() {},
                oui::Hotkey(oui::VirtualKey::kA)
            }
        };
        auto button = m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("help")),
            std::move(help)
        );
        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kH), [=]() { m_menu->SelectAndOpenPopup(button);  });
    }

}