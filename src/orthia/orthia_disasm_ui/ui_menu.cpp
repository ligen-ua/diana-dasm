#include "ui_main_window.h"
#include "oui_open_process_dialog.h"
#include "ui_help.h"
#include "oui_goto_dialog.h"
#include "formats/orthia_image_format.h"
#include "orthia_saved_process.h"
#include "oui_open_saved_dialog.h"

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
    // final result
    if (result.error.native.empty())
    {
        // no error; the model has logged what was opened
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

void CMainWindow::ShowShellcodeDialog(std::shared_ptr<oui::IFile2> file,
    oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler)
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
        [weakMe, file, completeHandler](DI_UINT64 address, int dianaMode, int dianaArch)
        {
            if (auto p = weakMe.lock())
                p->AsyncOpenFileAsShellcode(file, address, dianaMode, dianaArch, completeHandler);
        }
    ));
    if (completeHandler)
    {
        dialog->SetCancelHandler([completeHandler]()
        {
            auto node = g_textManager->QueryNodeDef(ORTHIA_TCSTR("model.errors"));
            oui::fsui::OpenResult result;
            result.error = node->QueryValue(ORTHIA_TCSTR("cancelled"));

            completeHandler->ReplyWithRetain(completeHandler,
                completeHandler, std::shared_ptr<oui::IFile2>(), result);
        });
    }
    dialog->SetCaption(node->QueryValue(ORTHIA_TCSTR("caption")));
    dialog->Dock();
}

bool CMainWindow::AsyncOpenFileAsShellcode(std::shared_ptr<oui::IFile2> file, DI_UINT64 baseAddress, int dianaMode, int dianaArch,
    oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> outerHandler)
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
        return false;

    auto innerHandler = std::make_shared<oui::Operation<oui::fsui::FileCompleteHandler_type>>(
        this->GetThread(),
        [=](std::shared_ptr<oui::BaseOperation> op, std::shared_ptr<oui::IFile> f, const oui::fsui::OpenResult& result) {
        if (auto p = weakMe.lock())
            p->OnFileOpen(f, result);
        if (outerHandler)
            outerHandler->ReplyWithRetain(outerHandler, outerHandler, std::shared_ptr<oui::IFile2>(), result);
    });

    m_model->GetFileSystem()->AsyncExecute(GetThread(),
        [file, baseAddress, dianaMode, dianaArch, model = m_model, innerHandler = std::move(innerHandler)]() {
        orthia::MapFileParameters params;
        params.imageBase = baseAddress;
        params.dianaMode = dianaMode;
        params.dianaArch = dianaArch;
        model->AddExecutable(file, innerHandler, orthia::FindFormatById("raw"), params);
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
        if (auto saved = std::dynamic_pointer_cast<orthia::CSavedProcess>(process))
        {
            model->AddSavedProcess(saved, completeHandler);
            return;
        }
        model->AddProcess(process, completeHandler);
    });
    return true;
}

oui::fsui::OpenResult CMainWindow::HandleOpenExecutable(std::shared_ptr<oui::COpenFileDialog> dialog,
    std::shared_ptr<oui::IFile2> file,
    oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> completeHandler,
    oui::OpenFileModelInfo modelInfo)
{ 
    if (dialog && file && completeHandler)
    {
        if (modelInfo.formatId.empty() && !(oui::model_flag_already_opened & modelInfo.flags))
        {
            ShowShellcodeDialog(file, completeHandler);
            return oui::fsui::OpenResult();
        }
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

        // a file opened by hand before is reopened by the model with the saved parameters
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
        [=](std::shared_ptr<oui::COpenFileDialog> dlg, std::shared_ptr<oui::IFile2> file, oui::OperationPtr_type<oui::fsui::FileCompleteHandler_type> handler, oui::OpenFileModelInfo modelInfo) {
            if (auto p = weakMe.lock())
            {
                return p->HandleOpenExecutable(dlg, file, handler, std::move(modelInfo));
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
void CMainWindow::OpenSavedProcess()
{
    auto me = oui::GetPtr_t<CMainWindow>(this);
    std::weak_ptr<CMainWindow> weakMe = me;
    if (!me)
    {
        return;
    }
    oui::CommonDialogStrings dialogStrings;
    GetCommonDialogStrings(ORTHIA_TCSTR("ui.dialog.opensaved"), dialogStrings);
    auto dialog = AddChildAndInit_t(std::make_shared<oui::COpenSavedProcessDialog>(dialogStrings,
        [model = m_model]() {
            return orthia::EnumerateProcDatabases(model->GetConfig()->GetProcDBFolder());
        },
        [=](const orthia::PlatformString_type& argument) {
            auto p = weakMe.lock();
            if (!p)
            {
                return oui::fsui::OpenResult(OUI_TCSTR("Error"));
            }
            try
            {
                // what --proc-folder takes; the folder is read here, the module files on the FS thread
                auto process = p->m_model->OpenSavedProcessFolder(argument);
                p->AsyncOpenProcess(process);
            }
            catch (const std::exception& e)
            {
                return oui::fsui::OpenResult(oui::String(orthia::Utf8ToPlatformString(e.what())));
            }
            return oui::fsui::OpenResult();
        }));
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
                uiMenuTextNodeFile->QueryValue(ORTHIA_TCSTR("open_saved")),
                [this]() {  OpenSavedProcess();  },
                oui::Hotkey(oui::VirtualKey::kS)
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
                    OnUIExit();
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