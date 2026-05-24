#include "ui_main_window.h"
#include "oui_open_process_dialog.h"
#include "ui_help.h"
#include "oui_goto_dialog.h"

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
void CMainWindow::OnFileOpen(std::shared_ptr<oui::IFile> file, const oui::fsui::OpenResult& result)
{
    // unknown format: offer shellcode loading
    auto scIt = result.extraInfo.find(orthia::model_OpenResult_extraInfo_CanOpenAsShellcode);
    if (scIt != result.extraInfo.end())
    {
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        m_outputWindow->AddLine(mainNode->QueryValue(ORTHIA_TCSTR("unknown")));
        ShowShellcodeDialog(std::any_cast<std::shared_ptr<oui::IFile2>>(scIt->second));
        return;
    }

    // final result
    if (result.error.native.empty())
    {
        // no error
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.main"));
        m_outputWindow->AddLine(mainNode->QueryValue(ORTHIA_TCSTR("done-opened")));

        OnWorkspaceItemChanged(result);
        return;
    }

    // error
    if (file)
    {
        auto mainNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
        m_outputWindow->AddLine(oui::PassParameter2(mainNode->QueryValue(ORTHIA_TCSTR("file-error-name-code")),
            file->GetFullFileNameForUI(),
            result.error));
    }
}

void CMainWindow::ShowShellcodeDialog(std::shared_ptr<oui::IFile2> file)
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    if (!me)
        return;

    std::weak_ptr<CMainWindow> weakMe = me;
    auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.shellcode"));

    auto dialog = AddChildAndInit_t(std::make_shared<oui::CShellcodeDialog>(
        [node]() { return node->QueryValue(ORTHIA_TCSTR("message")); },
        [node]() { return node->QueryValue(ORTHIA_TCSTR("addr-label")); },
        [node]() { return node->QueryValue(ORTHIA_TCSTR("mode-label")); },
        [weakMe, file](DI_UINT64 address, int dianaMode)
        {
            if (auto p = weakMe.lock())
                p->AsyncOpenFileAsShellcode(file, address, dianaMode);
        }
    ));
    dialog->SetCaption(node->QueryValue(ORTHIA_TCSTR("caption")));
    dialog->Dock();
}

bool CMainWindow::AsyncOpenFileAsShellcode(std::shared_ptr<oui::IFile2> file, DI_UINT64 baseAddress, int dianaMode)
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
        return false;

    auto completeHandler = std::make_shared<oui::Operation<oui::fsui::FileCompleteHandler_type>>(
        this->GetThread(),
        [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IFile> f, const oui::fsui::OpenResult& result) {
        if (auto p = weakMe.lock())
            p->OnFileOpen(f, result);
    });

    m_model->GetFileSystem()->AsyncExecute(GetThread(),
        [file, baseAddress, dianaMode, model = m_model, completeHandler = std::move(completeHandler)]() {
        model->AddExecutableAsShellcode(file, baseAddress, dianaMode, completeHandler);
    });
    return true;
}
bool CMainWindow::AsyncOpenFile(std::shared_ptr<oui::IFile2> file)
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return false;
    }
    auto completeHandler = std::make_shared<oui::Operation<oui::fsui::FileCompleteHandler_type>>(
        this->GetThread(),
        [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IFile> file, const oui::fsui::OpenResult& result) {
        if (auto p = weakMe.lock())
        {
            me->OnFileOpen(file, result);
        }
    });

    m_model->GetFileSystem()->AsyncExecute(GetThread(), [file, model = m_model, completeHandler = std::move(completeHandler)] {
        model->AddExecutable(file, completeHandler);
    });
    return true;
}

bool CMainWindow::AsyncOpenProcess(std::shared_ptr<oui::IProcess> process)
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return false;
    }
    auto completeHandler = std::make_shared<oui::Operation<oui::fsui::ProcessCompleteHandler_type>>(
        this->GetThread(),
        [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IProcess> proc, const oui::fsui::OpenResult& result) {
        if (auto p = weakMe.lock())
        {
            if (auto p = weakMe.lock())
            {
                p->OnFileOpen(proc, result);
            }
        }
    });

    m_model->GetFileSystem()->AsyncExecute(GetThread(), [process, model = m_model, completeHandler = std::move(completeHandler)] {
        model->AddProcess(process, completeHandler);
    });
    return true;
}

oui::fsui::OpenResult CMainWindow::HandleOpenExecutable(std::shared_ptr<oui::COpenFileDialog> dialog,
    std::shared_ptr<oui::IFile2> file,
    oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
    int fileType)
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
            [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IFile2> file, const oui::fsui::OpenResult& result) {
    
            if (auto p = weakMe.lock())
            {
                p->OnFileOpen(file, result);
            }
            rawHandler(op, file, result);
        });

        m_model->GetFileSystem()->AsyncExecute(dialog->GetThread(), [file, model = m_model, completeHandler = std::move(completeHandler)] {
            model->AddExecutable(file, completeHandler);
        });
    }
    return oui::fsui::OpenResult();
};

oui::fsui::OpenResult CMainWindow::HandleOpenProcess(std::shared_ptr<oui::COpenProcessDialog> dialog,
    std::shared_ptr<oui::IProcess> process,
    oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> completeHandler)
{
    if (dialog && process && completeHandler)
    {
        // means open dialog manager to open a file
        // setup UI proxy on success and pass the handler to FS handler
        auto me = oui::GetPtr_t<CMainWindow>(this);
        std::weak_ptr<CMainWindow> weakMe = me;
        if (!me)
        {
            return oui::fsui::OpenResult();
        }
        oui::fsui::ProcessCompleteHandler_type rawHandler = completeHandler->GetHandler();
        completeHandler->SetHandler(
            [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IProcess> proc, const oui::fsui::OpenResult& result) {

            if (auto p = weakMe.lock())
            {
                p->OnFileOpen(proc, result);
            }
            rawHandler(op, proc, result);
        });

        m_model->GetFileSystem()->AsyncExecute(dialog->GetThread(), [process, model = m_model, completeHandler = std::move(completeHandler)] {
            model->AddProcess(process, completeHandler);
        });
    }
    return oui::fsui::OpenResult();
}

void CMainWindow::OpenProcess()
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return;
    }

    // create open dialog
    oui::CommonDialogStrings dialogStrings;
    GetCommonDialogStrings(ORTHIA_TCSTR("ui.dialog.openprocess"), dialogStrings);

    int flags = oui::IProcessSystem::queryFlags_TryOpenProcessAsReader;
    auto dialog = AddChildAndInit_t(std::make_shared<oui::COpenProcessDialog>(dialogStrings,
        [=](std::shared_ptr<oui::COpenProcessDialog> dlg, std::shared_ptr<oui::IProcess> proc, oui::OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> handler) {
        if (auto p = weakMe.lock())
        {
            return p->HandleOpenProcess(dlg, proc, handler);
        }
        oui::fsui::OpenResult result(OUI_TCSTR("Error"));
        return result;
    },
        m_model->GetProcessSystem(),
        flags));
    dialog->Dock();
}
void CMainWindow::OpenExecutable()
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return;
    }
    
    // create open dialog
    oui::CommonDialogStrings dialogStrings;
    GetCommonDialogStrings(ORTHIA_TCSTR("ui.dialog.openfile"), dialogStrings);

    auto dialog = AddChildAndInit_t(std::make_shared<oui::COpenFileDialog>(oui::String(),
        dialogStrings,
        [=](std::shared_ptr<oui::COpenFileDialog> dlg, std::shared_ptr<oui::IFile2> file, oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> handler, int fileType) {
            if (auto p = weakMe.lock())
            {
                return p->HandleOpenExecutable(dlg, file, handler, fileType);
            }
            oui::fsui::OpenResult result(OUI_TCSTR("Error"));
            return result;
        },
        m_model->GetFileSystem(),
        [model = m_model](oui::ThreadPtr_type thread,
            const oui::FileUnifiedId& fileId,
            oui::FileWithTypeRecipientHandler_type resultCallback)
        {
            model->AsyncOpenFileWithType(thread, fileId, std::move(resultCallback));
        },
        oui::FileInfo::flag_any_executable));
    dialog->Dock();
}
void CMainWindow::ToggleWorkspaceView()
{
    m_workspaceWindow->SetVisible(!m_workspaceWindow->IsVisible());
    if (m_workspaceWindow->IsVisible())
    {
        m_workspaceWindow->OnWorkspaceItemChanged();
        m_workspaceWindow->SetFocus();
    }
}
void CMainWindow::ShowAbout()
{
    auto waitBox = AddChildAndInit_t(std::make_shared<oui::CAboutBoxWindow>());
    waitBox->Dock();
}
void CMainWindow::ShowHelp()
{
    auto waitBox = AddChildAndInit_t(std::make_shared<oui::CHelpWindow>());
    waitBox->Dock();
}
void CMainWindow::OpenHistoryModal()
{
    // create open dialog
    oui::CommonDialogStrings dialogStrings;
    GetCommonDialogStrings(ORTHIA_TCSTR("ui.dialog.goto"), dialogStrings);

    auto activeItem = m_model->GetActiveItem();
    if (!activeItem)
    {
        return;
    }
    if (!activeItem->GetPersistentStorage())
    {
        return;
    }
    auto dialog = AddChildAndInit_t(std::make_shared<oui::CGotoDialog>(dialogStrings,
        [=](orthia::Address_type address, int error) {

        if (!error)
        {
            m_panelContainerWindow->GetDefaultGroup()->SwitchPanel(m_disasmWindow);
            m_disasmWindow->DoGotoRequest(address);
        }
        return oui::fsui::OpenResult();
    },
        activeItem->GetPersistentStorage(),
        activeItem,
        orthia::IPeristentItemStorage::goto_flags_history_mode));
    dialog->Dock();
}
void CMainWindow::ConstuctMenu()
{
    auto uiMenuTextNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu"));
    auto uiMenuTextNodeFile = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu.file"));
    auto uiMenuTextNodeView = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.menu.view"));

    m_menu = AddChild_t(std::make_shared<oui::CMenuWindow>());
    m_menu->SetBackgroundColor(oui::ColorBlack());


    m_hotkeys.Register(oui::Hotkey(oui::KeyState(),
        oui::VirtualKey::kF1), [=]() { ShowHelp();  });
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
                [this]() {  OpenProcess();  },
                oui::Hotkey(oui::VirtualKey::kP)
            },
            {
                uiMenuTextNodeFile->QueryValue(ORTHIA_TCSTR("close_item")),
                [this]() { CloseCurrentItem(); },
                oui::Hotkey(oui::VirtualKey::kC),
                [this]() -> bool { return m_model->GetActiveItemId() != 0; }
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
                uiMenuTextNodeView->QueryValue(ORTHIA_TCSTR("workspace")),
                [this]() { ToggleWorkspaceView(); },
                oui::Hotkey(oui::VirtualKey::kW)
            },
            {
                uiMenuTextNodeView->QueryValue(ORTHIA_TCSTR("history")),
                [this]() { OpenHistoryModal(); },
                oui::Hotkey(oui::VirtualKey::kH)
            }
        };
        auto button = m_menu->AddButton(uiMenuTextNode->QueryValue(ORTHIA_TCSTR("view")),
            std::move(view)
        );

        m_hotkeys.Register(oui::Hotkey(oui::KeyState(oui::KeyState::AnyAlt),
            oui::VirtualKey::kV), [=]() { m_menu->SelectAndOpenPopup(button, false);  });
    }
    {
        std::vector<oui::PopupItem> help =
        {
            {
                OUI_TCSTR("&Help"),
                [this]() { ShowHelp(); },
                oui::Hotkey(oui::VirtualKey::kH)
            },
            {
                OUI_TCSTR("&About"),
                [this]() { ShowAbout();  },
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