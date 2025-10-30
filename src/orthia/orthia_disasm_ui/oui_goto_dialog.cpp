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
            m_filesBox->Resize(zeroSize);
            m_fileEdit->Resize(zeroSize);
            m_fileLabel->Resize(zeroSize);
            return;
        }

        Rect boxRect = clientRect;
        boxRect.position.x += 2;
        boxRect.position.y += 4;
        boxRect.size.width -= 4;
        boxRect.size.height -= 4;

        m_filesBox->MoveTo(boxRect.position);
        m_filesBox->Resize(boxRect.size);

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
            m_filesBox->Clear();

            m_currentFilter = filter;
            m_currentItems.clear();
            m_currentItems.reserve(data.size() + 1);
            m_firstResult = false;

            m_parentOffset = m_filesBox->GetOffset();
            m_parentPosition = m_filesBox->GetSelectedPosition();

            m_filesBox->SetOffset(0);
            m_filesBox->SetSelectedPosition(0);

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
        UpdateVisibleItems();
    }

    void CGotoDialog::UpdateVisibleItems()
    {
        DefaultUpdateVisibleItems(this, this, m_filesBox, m_currentItems,
            [&](auto it, auto vit)
        {
            vit->text.clear();
            vit->text.push_back(orthia::ToWideStringAsHex(it->info.address));

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

            m_fileSystem->AsyncUpdateGotoInfo(this->GetThread(),
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

            m_fileSystem->AsyncQueryGotoInfo(this->GetThread(),
                m_currentFilter,
                operation,
                0
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
        int scanFlags)
        :
            m_resultCallback(resultCallback),
            m_fileSystem(fileSystem),
            m_openingText(dialogStrings.openingText),
            m_errorText(dialogStrings.errorText),
            m_scanFlags(scanFlags)
    {
        SetCaption(dialogStrings.caption);

        IListBoxOwner* owner = this;
        m_filesBox = std::make_shared<CListBox>(m_colorProfile, owner);

        auto columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.goto.columns"));
        m_filesBox->InitColumns(oui::ColumnParam([=] { return columnsNode->QueryValue(L"address");  }),
            oui::ColumnParam([=] { return columnsNode->QueryValue(L"comment");  })
        );
        m_fileEdit = std::make_shared<CEditBox>(m_colorProfile);
        m_fileEdit->SetEnterHandler([this](const String& text) {

            try
            {
                orthia::Address_type address = oui::CaptureAddress(text.native);
                TryOpenAddress(address);
            }
            catch (std::exception& e)
            {
                oui::String error(orthia::Utf8ToUtf16(e.what()));
                
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
        this->RegisterSwitch(m_filesBox);
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
        AddChild(m_filesBox);
        AddChild(m_fileEdit);
        AddChild(m_fileLabel);
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
        if (!m_filesBox->GetSelectedItem(item))
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
        int maxVisibleOffset = std::min(m_filesBox->GetVisibleSize() + m_filesBox->GetOffset(), (int)m_currentItems.size());
        if (highlightItemOffset >= m_filesBox->GetOffset() && highlightItemOffset < maxVisibleOffset)
        {
            m_filesBox->SetSelectedPosition(highlightItemOffset - m_filesBox->GetOffset());
        }
        else
        {
            // try to reuse at least position
            int newOffset = highlightItemOffset - m_filesBox->GetSelectedPosition();
            if (newOffset < 0)
            {
                newOffset = 0;
                m_filesBox->SetSelectedPosition(highlightItemOffset);
            }
            else if (newOffset >= (int)m_currentItems.size())
            {
                newOffset = highlightItemOffset;
                m_filesBox->SetSelectedPosition(0);
            }
            m_filesBox->SetOffset(newOffset);
        }
    }
    bool CGotoDialog::ShiftViewWindowToSymbol(const String& symbol) 
    {
        const int totalProcessAvailable = (int)m_currentItems.size();
        const int selectionOffset = m_filesBox->GetOffset() + m_filesBox->GetSelectedPosition();

        // scan forward till end
        for (int i = selectionOffset + 1; i < totalProcessAvailable; ++i)
        {
            if (StartsWith(orthia::ObjectToString(m_currentItems[i].info.address), symbol.native))
            {
                HighlightItem(i);
                UpdateVisibleItems();
                return true;
            }
        }

        // scan from start
        for (int i = 0; i <= std::min((int)m_currentItems.size() - 1, selectionOffset); ++i)
        {
            if (StartsWith(orthia::ObjectToString(m_currentItems[i].info.address), symbol.native))
            {
                HighlightItem(i);
                UpdateVisibleItems();
                return true;
            }
        }
        return false;
    }

    void CGotoDialog::ShiftViewWindow(int newOffset)
    {
        DefaultShiftViewWindow(m_filesBox, newOffset, m_currentItems.size());
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
                    m_filesBox->SetFocus();
                    return true;
                }
            }
            
            if (m_filesBox->IsFocused())
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
