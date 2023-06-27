#define  _CRT_SECURE_NO_WARNINGS
#include "oui_open_process_dialog.h"

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
    static void GenSortKey(ProcessDialogInfo& fileInfo)
    {
        auto& info = fileInfo.info;
        fileInfo.sortKey.native.clear();

        String::string_type fixedStr(20, OUI_TCHAR('0'));
        auto pidStr = OUI_TO_STR(fileInfo.info.pid);

        std::copy(pidStr.begin(), pidStr.end(), fixedStr.begin() + (fixedStr.size() - std::min(fixedStr.size(), pidStr.size())));
        fileInfo.sortKey = std::move(fixedStr);
    }
    void COpenProcessDialog::OnResize()
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
    void COpenProcessDialog::CancelAllQueries()
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
            return;
        }
    }
    void COpenProcessDialog::OnOpCompleted(std::shared_ptr<BaseOperation> operation,
        const ProcessUnifiedId& folderId,
        const std::vector<ProcessInfo>& data,
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

            m_currentFolderId = folderId;
            m_currentProcess.clear();
            m_currentProcess.reserve(data.size() + 1);
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
            m_currentProcess.reserve(m_currentProcess.size() + data.size());
        }

        for (auto& info : data)
        {
            m_currentProcess.push_back(info);
            GenSortKey(m_currentProcess.back());
            m_currentProcess.back().visibleName = info.processName;
            console->FilterOrReplaceUnreadableSymbols(m_currentProcess.back().visibleName);
        }
        std::sort(m_currentProcess.begin(), m_currentProcess.end());
        UpdateVisibleItems();
    }

    void COpenProcessDialog::UpdateVisibleItems()
    {
        auto console = GetConsole();
        if (!console)
        {
            return;
        }
        // update visible items
        const auto visibleSize = m_filesBox->GetVisibleSize();
        auto& visibleItems = m_filesBox->GetItems();
        const int maxSize = (int)m_currentProcess.size();

        auto offset = m_filesBox->GetOffset();
        if (offset >= maxSize)
        {
            // set to the last file here
            visibleItems.clear();
            if (m_currentProcess.empty())
            {
                m_filesBox->Invalidate();
                return;
            }
            // we have some files, show last page
            offset = (int)m_currentProcess.size() - visibleSize;
            if (offset < 0)
            {
                offset = 0;
            }
            m_filesBox->SetOffset(offset);
        }

        auto sizeToProceed = std::min(maxSize - offset, visibleSize);
        visibleItems.resize(sizeToProceed);

        auto it = m_currentProcess.begin() + offset;
        auto it_end = it + sizeToProceed;
        auto vit = visibleItems.begin();
        for (; it != it_end; ++it, ++vit)
        {
            vit->text.clear();
            vit->text.push_back(it->visibleName);
            vit->fsFlags = it->info.flags;

            // open file here
            vit->openHandler = [=, info = it->info]() {
                TryOpenProcess(info.pid);
            };
            vit->colorsHandler = nullptr;
        }
        OnVisibleItemChanged();
        m_filesBox->Invalidate();
    }
    String COpenProcessDialog::GetWaitBoxText()
    {
        return m_waitBoxText;
    }
    void COpenProcessDialog::TryOpenProcess(const ProcessUnifiedId& folderId)
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
        }

        auto me = GetPtr_t<COpenProcessDialog>(this);
        if (!me)
        {
            return;
        }
        if (m_waitBox)
        {
            return;
        }
        m_waitBoxText = PassParameter1(m_openingText, ToString(folderId));
        std::weak_ptr<COpenProcessDialog> weakMe = me;
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


        auto operation = std::make_shared<Operation<QueryProcessHandler_type>>(
            this->GetThread(),
            std::bind(&COpenProcessDialog::OnOpCompleted, this,
                std::placeholders::_1,
                std::placeholders::_2,
                std::placeholders::_3,
                std::placeholders::_4));

        m_currentOperation = operation;
        m_firstResult = true;

        m_fileSystem->AsyncStartQueryProcess(this->GetThread(),
            folderId,
            [=, openProcessSeq = m_openProcessSeq](std::shared_ptr<IProcess> file, int error) {
                if (auto p = weakMe.lock())
                {
                    me->SetOpenProcessResult(openProcessSeq, file, error);
                }
            },
            operation,
            0);
    }
    void COpenProcessDialog::FinishProcessOpen(std::shared_ptr<BaseOperation> op, const oui::fsui::OpenResult& result)
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

        // the owner doesn't approve this particular file
        // show it
        m_result = nullptr;
        if (m_waitBox)
        {
            m_waitBoxText = result.error.native;
            m_waitBox->Invalidate();
        }
    }
    void COpenProcessDialog::SetOpenProcessResult(int openProcessSeq, std::shared_ptr<IProcess> file, int error)
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
        m_result = file;
        auto me = GetPtr_t<COpenProcessDialog>(this);
        if (m_resultCallback && me)
        {
            // call approve callback
            std::weak_ptr<COpenProcessDialog> weakMe = me;
            auto operation = std::make_shared<Operation<oui::fsui::ProcessCompleteHandler_type>>(
                this->GetThread(),
                [=](std::shared_ptr<BaseOperation> op, std::shared_ptr<IProcess> file, const oui::fsui::OpenResult& result) {
                if (auto p = weakMe.lock())
                {
                    me->FinishProcessOpen(op, result);
                }
            });
            m_openOperation = operation;
            auto errorText = m_resultCallback(me, m_result, m_openOperation);
            if (errorText.error.native.empty())
            {
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
    void COpenProcessDialog::OnWaitBoxDestroyed()
    {
        m_waitBox = 0;
        m_result = nullptr;
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
   
    COpenProcessDialog::COpenProcessDialog(const String& openingText,
        const String& errorText,
        ProcessRecipientHandler_type resultCallback,
        std::shared_ptr<IProcessSystem> fileSystem)
        :
            m_resultCallback(resultCallback),
            m_fileSystem(fileSystem),
            m_openingText(openingText),
            m_errorText(errorText)
    {
        IListBoxOwner* owner = this;
        m_filesBox = std::make_shared<CListBox>(m_colorProfile, owner);
        m_filesBox->InitColumns(2);

        m_fileEdit = std::make_shared<CEditBox>(m_colorProfile);
        m_fileEdit->SetEnterHandler([this](const String& text) {
            TryOpenProcess(ProcessUnifiedId(text));
        });
        m_fileEdit->SetSelectAllOnFocus(true);
        m_fileLabel = std::make_shared<CLabel>(m_colorProfile, [] { return String(OUI_STR(">"));  });

        this->RegisterSwitch(m_fileEdit);
        this->RegisterSwitch(m_filesBox);
    }
    COpenProcessDialog::~COpenProcessDialog()
    {

    }
    void COpenProcessDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_fileEdit->SetFocus();
        TryOpenProcess(oui::ProcessUnifiedId());
    }
    void COpenProcessDialog::ConstructChilds()
    {
        AddChild(m_filesBox);
        AddChild(m_fileEdit);
        AddChild(m_fileLabel);
    }
    void COpenProcessDialog::OnFinishDialog()
    {
        if (m_waitBox)
        {
            m_waitBox->Destroy();
        }
        if (m_resultCallback && !m_result)
        {
            // report nothing
            auto me = GetPtr_t<COpenProcessDialog>(this);
            m_resultCallback(me, m_result, nullptr);
            m_resultCallback = nullptr;
        }
        Parent_type::OnFinishDialog();
    }

    void COpenProcessDialog::OnVisibleItemChanged()
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
    void COpenProcessDialog::HighlightItem(int highlightItemOffset)
    {
        int maxVisibleOffset = std::min(m_filesBox->GetVisibleSize() + m_filesBox->GetOffset(), (int)m_currentProcess.size());
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
            else if (newOffset >= (int)m_currentProcess.size())
            {
                newOffset = highlightItemOffset;
                m_filesBox->SetSelectedPosition(0);
            }
            m_filesBox->SetOffset(newOffset);
        }
    }
    bool COpenProcessDialog::ShiftViewWindowToSymbol(const String& symbol) 
    {
        const int totalProcessAvailable = (int)m_currentProcess.size();
        const int selectionOffset = m_filesBox->GetOffset() + m_filesBox->GetSelectedPosition();

        // scan forward till end
        for (int i = selectionOffset + 1; i < totalProcessAvailable; ++i)
        {
            if (StartsWith(m_currentProcess[i].info.processName.native, symbol.native))
            {
                HighlightItem(i);
                UpdateVisibleItems();
                return true;
            }
        }

        // scan from start
        for (int i = 0; i <= selectionOffset; ++i)
        {
            if (StartsWith(m_currentProcess[i].info.processName.native, symbol.native))
            {
                HighlightItem(i);
                UpdateVisibleItems();
                return true;
            }
        }
        return false;
    }

    void COpenProcessDialog::ShiftViewWindow(int newOffset)
    {
        const int visibleSize = m_filesBox->GetVisibleSize();
        const int totalProcessAvailable = (int)m_currentProcess.size();

        int newSelectedPositon = m_filesBox->GetSelectedPosition();

        int newSelectedOffset = newSelectedPositon + newOffset;
        int maxOffset = totalProcessAvailable - visibleSize;
        if (maxOffset < 0)
        {
            maxOffset = 0;
        }
        if (newOffset > maxOffset)
        {
            if (newSelectedOffset >= totalProcessAvailable)
            {
                auto sizeToProceed = std::min(totalProcessAvailable - maxOffset, visibleSize);
                newSelectedPositon = sizeToProceed - 1;
            }
            else
            {
                newSelectedPositon = visibleSize - (totalProcessAvailable - newSelectedOffset);
            }
            newOffset = maxOffset;
        }
        if (newOffset < 0)
        {
            newOffset = 0;
            newSelectedPositon = newSelectedOffset;
            if (newSelectedPositon < 0)
            {
                newSelectedPositon = 0;
            }
        }
        m_filesBox->SetSelectedPosition(newSelectedPositon);
        m_filesBox->SetOffset(newOffset);
        UpdateVisibleItems();
    }
    int COpenProcessDialog::GetTotalCount() const
    {
        return (int)m_currentProcess.size();
    }

}