#define  _CRT_SECURE_NO_WARNINGS
#include "oui_goto_dialog.h"
#include "ui_common.h"

namespace oui
{
    static oui::String ToString(const ProcessUnifiedId& info)
    {
        if (info.pid)
        {
            return OUI_TO_STR(info.pid);
        }
        return info.namePart;
    }
    static void GenSortKey(GotoDialogInfo& fileInfo)
    {
        fileInfo.sortKey.native.clear();
        fileInfo.sortKey.native.append(orthia::ToWideStringAsHex(~fileInfo.info.lastUpdateTime.ToLongLongTime()));
        fileInfo.sortKey.native.append(OUI_TCSTR("@"));
        fileInfo.sortKey.native.append(orthia::ToWideStringAsHex(fileInfo.info.address));
    }
    void CGotoDialog::OnResize()
    {
        const auto clientRect = GetClientRect();

        if (clientRect.size.width < 5 || clientRect.size.height < 5)
        {
            Size zeroSize;
            m_listBoxScrollable->Resize(zeroSize);
            m_fileEdit->Resize(zeroSize);
            m_fileLabel->Resize(zeroSize);
            return;
        }

        Rect boxRect = clientRect;
        boxRect.position.x += 2;
        boxRect.position.y += 4;
        boxRect.size.width -= 4;
        boxRect.size.height -= 4;

        auto listBoxSize = m_listBox->GetSize();
        if (listBoxSize.width < boxRect.size.width)
        {
            m_listBox->Resize(boxRect.size);
        }

        m_listBoxScrollable->MoveTo(boxRect.position);
        m_listBoxScrollable->Resize(boxRect.size);

        // file edit
        Rect fileEditRect = clientRect;
        fileEditRect.position.x += 3;
        fileEditRect.position.y += 2;
        fileEditRect.size.width -= 5;
        fileEditRect.size.height = 1;

        m_fileEdit->MoveTo(fileEditRect.position);
        m_fileEdit->Resize(fileEditRect.size);

        // small 1 symbol label
        Rect labelRect = fileEditRect;
        --labelRect.position.x;
        labelRect.size.width = 1;
        labelRect.size.height = 1;
        m_fileLabel->MoveTo(labelRect.position);
        m_fileLabel->Resize(labelRect.size);

        Parent_type::OnResize();
    }
    void CGotoDialog::CancelAllQueries()
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
            return;
        }
    }
    void CGotoDialog::OnOpCompleted(std::shared_ptr<BaseOperation> operation,
        const oui::String& filter,
        const std::vector<orthia::GotoItem>& data,
        int error)
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        if (operation != m_currentOperation)
        {
            return;
        }
        if (m_waitBox)
        {
            m_waitBox->FinishDialog();
            m_waitBox = 0;
        }
        if (error && data.empty())
        {
            return;
        }
        if (m_firstResult)
        {
            m_listBox->Clear();

            m_currentFilter = filter;
            m_currentItems.clear();
            m_currentItems.reserve(data.size() + 1);
            m_firstResult = false;

            m_parentOffset = m_listBox->GetOffset();
            m_parentPosition = m_listBox->GetSelectedPosition();

            m_listBox->SetOffset(0);
            m_listBox->SetSelectedPosition(0);

            m_fileEdit->ScrollRight();
            m_fileEdit->SetFocus();
        }
        else
        {
            m_currentItems.reserve(m_currentItems.size() + data.size());
        }

        for (auto& info : data)
        {
            m_currentItems.push_back(info);
            m_currentItems.back().info = info;
            GenSortKey(m_currentItems.back());
            console->FilterOrReplaceUnreadableSymbols(m_currentItems.back().visibleName);
        }
        std::sort(m_currentItems.begin(), m_currentItems.end());

        for (auto& item : m_currentItems)
        {
            if (item.info.comment.native.empty())
            {
                item.info.comment = m_workPlace->QueryAddressName(item.info.address);
            }
        }
        UpdateVisibleItems();
    }

    void CGotoDialog::UpdateVisibleItems()
    {
        DefaultUpdateVisibleItems(this, this, m_listBox, m_currentItems,
            [&](auto it, auto vit)
        {
            vit->text.clear();
            vit->text.push_back(orthia::ToWideStringAsHex(it->info.address));
            vit->text.push_back(it->info.comment);

            // open file here
            vit->openHandler = [=, info = it->info]() {
                TryOpenAddress(it->info.address);
            };

            vit->colorsHandler = nullptr;
            vit->colorsHandler = [=]() { return LabelColorState{ m_colorProfile->listBoxFolders, Color() }; };            
        });
    }
    String CGotoDialog::GetWaitBoxText()
    {
        return m_waitBoxText;
    }
    void CGotoDialog::TryOpenAddress(orthia::Address_type address)
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
        }

        auto me = GetPtr_t<CGotoDialog>(this);
        if (!me)
        {
            return;
        }
        if (m_waitBox)
        {
            return;
        }
        m_waitBoxText = PassParameter1(m_openingText, orthia::ToWideStringAsHex(address));
        std::weak_ptr<CGotoDialog> weakMe = me;
        m_waitBox = AddChildAndInit_t(std::make_shared<CMessageBoxWindow>(
            [=]() {
            if (auto p = weakMe.lock())
            {
                return p->GetWaitBoxText();
            }
            return String();
        },
            [=]() { 
            if (auto p = weakMe.lock())
            {
                return p->OnWaitBoxDestroyed();
            }
        }));
        m_waitBox->Dock();

        ++m_openProcessSeq;
        if (address) 
        {
            auto operation = std::make_shared<Operation<orthia::GotoCompleteHandler_type>>(
                this->GetThread(),
                [=, openProcessSeq = m_openProcessSeq](orthia::Address_type address, int error) {

                me->SetOpenProcessResult(openProcessSeq, address, error);
                return oui::fsui::OpenResult();
            });

            m_currentOperation = operation;
            m_firstResult = true;

            m_persistentStorage->AsyncUpdateGotoInfo(this->GetThread(),
                operation,
                address,
                0,
                0
            );
        }
        else 
        {
            auto operation = std::make_shared<Operation<orthia::QueryGotoItemHandler_type>>(
                this->GetThread(),
                [=, openProcessSeq = m_openProcessSeq](std::shared_ptr<oui::BaseOperation> operation,
                    const oui::String& filter,
                    const std::vector<orthia::GotoItem>& data,
                    int error) {

                OnOpCompleted(operation,
                    filter,
                    data,
                    error);

                return oui::fsui::OpenResult();
            });

            m_currentOperation = operation;
            m_firstResult = true;

            m_persistentStorage->AsyncQueryGotoInfo(this->GetThread(),
                m_currentFilter,
                operation,
                m_scanFlags
            );
        }
    }
    void CGotoDialog::FinishProcessOpen(std::shared_ptr<BaseOperation> op, const oui::fsui::OpenResult& result)
    {
        if (op != m_openOperation || !m_waitBox)
        {
            // user cancelled the op
            return;
        }
        // this is valid op
        if (result.error.native.empty())
        {
            // no error
            m_readyToExit = true;
            m_resultCallback = nullptr;
            m_waitBox->FinishDialog();
            return;
        }

        // the owner doesn't approve this particular address
        // show it
        m_result = 0;
        if (m_waitBox)
        {
            m_waitBoxText = result.error.native;
            m_waitBox->Invalidate();
        }
    }
    void CGotoDialog::SetOpenProcessResult(int openProcessSeq, orthia::Address_type address, int error)
    {
        if (openProcessSeq != m_openProcessSeq)
        {
            return;
        }
        if (error)
        {
            if (m_waitBox)
            {
                m_waitBoxText = PassParameter1(m_errorText, GetErrorText(error));
                m_waitBox->Invalidate();
            }
            return;
        }
        m_result = address;
        auto me = GetPtr_t<CGotoDialog>(this);
        if (m_resultCallback && me)
        {
            // call approve callback
            auto errorText = m_resultCallback(m_result, error);
            if (errorText.error.native.empty())
            {
                m_readyToExit = true;
                m_resultCallback = nullptr;
                if (m_waitBox)
                {
                    m_waitBox->FinishDialog();
                }
                else
                {
                    FinishDialog();
                }
                return;
            }
            if (m_waitBox)
            {
                m_waitBoxText = errorText.error;
                m_waitBox->Invalidate();
            }
            return;
        }
        m_readyToExit = true;
        m_resultCallback = nullptr;
        if (m_waitBox)
        {
            m_waitBox->FinishDialog();
        }
        else
        {
            FinishDialog();
        }
    }
    void CGotoDialog::OnWaitBoxDestroyed()
    {
        m_waitBox = 0;
        m_result = 0;
        if (m_openOperation)
        {
            m_openOperation->Cancel();
            m_openOperation = nullptr;
        }
        if (m_readyToExit)
        {
            FinishDialog();
        }
    }
   
    CGotoDialog::CGotoDialog(const oui::CommonDialogStrings& dialogStrings,
        orthia::GotoCompleteHandler_type resultCallback,
        std::shared_ptr<orthia::IPeristentItemStorage> fileSystem,
        std::shared_ptr<orthia::IWorkPlaceItem> workPlace,
        int scanFlags)
        :
            m_resultCallback(resultCallback),
            m_persistentStorage(fileSystem),
            m_openingText(dialogStrings.openingText),
            m_errorText(dialogStrings.errorText),
            m_scanFlags(scanFlags),
            m_workPlace(workPlace)
    {
        SetCaption(dialogStrings.caption);

        IListBoxOwner* owner = this;
        m_listBox = std::make_shared<CListBox>(m_colorProfile, owner);

        m_listBoxScrollable = std::make_shared<oui::CScrollable>(m_listBox);

        auto columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.goto.columns"));
        m_listBox->InitColumns(oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("address"));  }, 30),
            oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("comment"));  }, 60)
        );
        m_listBox->Dock();

        m_fileEdit = std::make_shared<CEditBox>(m_colorProfile);
        m_fileEdit->SetEnterHandler([this](const String& text) {

            try
            {
                orthia::Address_type address = oui::CaptureAddressExp(text.native, m_workPlace);
                TryOpenAddress(address);
            }
            catch (std::exception& e)
            {
                oui::String error(orthia::Utf8ToPlatformString(e.what()));
                
                auto waitBox = AddChildAndInit_t(std::make_shared<CMessageBoxWindow>(
                    [=]() {
                        return error;
                },
                    [=]() {
                    
                }));
                waitBox->Dock();
            }
        });
        m_fileEdit->SetSelectAllOnFocus(true);

        auto labelProfile = std::shared_ptr<LabelColorProfile>(m_colorProfile, &m_colorProfile->label);
        m_fileLabel = std::make_shared<CLabel>(labelProfile, [] { return String(OUI_STR(">"));  });

        this->RegisterSwitch(m_fileEdit);
        this->RegisterSwitch(m_listBox);
    }
    CGotoDialog::~CGotoDialog()
    {

    }
    void CGotoDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_fileEdit->SetFocus();
        TryOpenAddress(0);
    }
    void CGotoDialog::ConstructChilds()
    {
       // AddChild(m_listBox);
        AddChild(m_fileEdit);
        AddChild(m_fileLabel);    
        AddChild(m_listBoxScrollable);
    }
    void CGotoDialog::OnFinishDialog()
    {
        if (m_waitBox)
        {
            m_waitBox->Destroy();
        }
        Parent_type::OnFinishDialog();
    }

    void CGotoDialog::OnVisibleItemChanged()
    {
        ListBoxItem item;
        if (!m_listBox->GetSelectedItem(item))
        {
            return;
        }
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        if (item.text.empty())
        {
            return;
        }
        String newText;
        newText.native.append(item.text[0].native);
        m_fileEdit->SetText(newText);
        m_fileEdit->ScrollRight();
        m_fileEdit->Invalidate();
    }
    void CGotoDialog::HighlightItem(int highlightItemOffset)
    {
        DefaultHighlightItem(m_listBox, highlightItemOffset, m_currentItems.size());
    }
    bool CGotoDialog::ShiftViewWindowToSymbol(const String& symbol) 
    {
        return DefaultShiftViewWindowToSymbol(this, m_listBox, symbol, m_currentItems,
            [](const GotoDialogInfo& info, const String& symbol) 
                { 
                  return StartsWith(orthia::ObjectToString(info.info.address), symbol.native) ||
                      StartsWith(info.info.comment.native, symbol.native);
        });
    }

    void CGotoDialog::ShiftViewWindow(int newOffset)
    {
        DefaultShiftViewWindow(m_listBox, newOffset, m_currentItems.size());
        UpdateVisibleItems();
    }
    int CGotoDialog::GetTotalCount() const
    {
        return (int)m_currentItems.size();
    }
    bool CGotoDialog::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        // it is nice thing to use arrow to go from edit to box
        if (evt.keyEvent.valid)
        {
            if (evt.keyEvent.virtualKey == VirtualKey::Down)
            {
                if (m_fileEdit->IsFocused())
                {
                    m_listBox->SetFocus();
                    return true;
                }
            }
            
            if (m_listBox->IsFocused())
            {
                if (!evt.keyEvent.rawText.native.empty())
                {
                    m_fileEdit->SetFocus();
                    m_fileEdit->ProcessEvent(evt, evtContext);
                }
                return true;
            }
        }
    
        return Parent_type::ProcessEvent(evt, evtContext);
    }
}
