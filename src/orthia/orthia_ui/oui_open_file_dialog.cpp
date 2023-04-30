#define  _CRT_SECURE_NO_WARNINGS
#include "oui_open_file_dialog.h"

namespace oui
{

    static std::wstring Uppercase_Silent(const std::wstring& str)
    {
        if (str.empty())
            return std::wstring();

        std::vector<wchar_t> temp(str.c_str(), str.c_str() + str.size());
        DWORD dwSize = (DWORD)(str.size());
        if (CharUpperBuffW(&temp.front(), dwSize) != dwSize)
        {
            return str;
        }
        return std::wstring(&temp.front(), &temp.front() + dwSize);
    }
    static void GenSortKey(FileDialogInfo& fileInfo)
    {
        auto& info = fileInfo.info;
        fileInfo.sortKey.native.clear();
        fileInfo.sortKey.native.reserve(10 + info.fileName.native.size()*2);
        if (info.flags & info.flag_directory)
        {
            fileInfo.sortKey.native.append(L"0|");
        }
        else
        {
            fileInfo.sortKey.native.append(L"1|");
        }
        fileInfo.sortKey.native.append(Uppercase_Silent(info.fileName.native));
        fileInfo.sortKey.native.append(L"|");
        fileInfo.sortKey.native.append(info.fileName.native);
    }
    void COpenFileDialog::OnResize()
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
    void COpenFileDialog::AsyncQuery(CListBox* listBox, 
        std::function<void(const ListBoxItem*, int)> handler, 
        int offset, int size)
    {
    }
    void COpenFileDialog::CancelAllQueries()
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
            return;
        }
    }
    void COpenFileDialog::OnOpCompleted(std::shared_ptr<BaseOperation> operation,
        const FileUnifiedId& folderId,
        const std::vector<FileInfo>& data,
        int error,
        const String& tag)
    {
        if (operation != m_currentOperation)
        {
            return;
        }
        if (error && data.empty())
        {
            return;
        }
        if (m_firstResult)
        {
            m_filesBox->Clear();

            m_currentFolderId = folderId;
            m_currentFiles.clear();
            m_currentFiles.reserve(data.size() + 1);
            m_firstResult = false;

            m_fileEdit->SetText(m_fileSystem->AppendSlash(m_currentFolderId.fullFileName));
            m_fileEdit->ScrollRight();

            if (!m_currentFolderId.fullFileName.native.empty())
            {
                FileInfo info;
                info.fileName = OUI_STR("..");
                info.flags |= info.flag_directory | info.flag_uplink;
                m_currentFiles.push_back(info);
            }
            m_parentOffset = m_filesBox->GetOffset();
            m_parentPosition = m_filesBox->GetSelectedPosition();

            m_filesBox->SetOffset(0);
            m_filesBox->SetSelectedPosition(0);
        }
        else
        {
            m_currentFiles.reserve(m_currentFiles.size() + data.size());
        }

        String highlighName;
        bool applyTag = false;
        for (auto& info : data)
        {
            m_currentFiles.push_back(info);
            GenSortKey(m_currentFiles.back());

            if (info.flags & info.flag_highlight)
            {
                highlighName = info.fileName;
                applyTag = true;
            }
        }
        if (highlighName.native.empty())
        {
            oui::ListBoxItem listItem;
            if (m_filesBox->GetSelectedItem(listItem) && !listItem.text.empty())
            {
                highlighName = listItem.text[0];
            }
        }
        std::sort(m_currentFiles.begin(), m_currentFiles.end());
        if (!highlighName.native.empty())
        {
            if (applyTag)
            {
                int offset = -1;
                int pos = -1;
                OUI_SCANF(tag.native.c_str(), OUI_TCSTR("%i:%i"), &offset, &pos);
                if (offset != -1 && pos != -1)
                {
                    if (pos < (int)m_currentFiles.size())
                    {
                        m_filesBox->SetSelectedPosition(pos);
                    }
                    if (offset < (int)m_currentFiles.size())
                    {
                        m_filesBox->SetOffset(offset);
                    }
                }
            }
            auto it = std::find_if(m_currentFiles.begin(), m_currentFiles.end(), [&](auto& value) { return value.info.fileName.native == highlighName.native;  });
            int highlightItemOffset = (int)(it - m_currentFiles.begin());
            int maxVisibleOffset = std::min(m_filesBox->GetVisibleSize() + m_filesBox->GetOffset(), (int)m_currentFiles.size());
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
                else if (newOffset >= (int)m_currentFiles.size())
                {
                    newOffset = highlightItemOffset;
                    m_filesBox->SetSelectedPosition(0);
                }
                m_filesBox->SetOffset(newOffset);
            }
        }
        UpdateVisibleItems();
    }
    void COpenFileDialog::UpdateVisibleItems()
    {
        // update visible items
        const auto visibleSize = m_filesBox->GetVisibleSize();
        auto& visibleItems = m_filesBox->GetItems();
        const int maxSize = (int)m_currentFiles.size();

        auto offset = m_filesBox->GetOffset();
        if (offset >= maxSize)
        {
            // set to the last file here
            visibleItems.clear();
            if (m_currentFiles.empty())
            {
                m_filesBox->Invalidate();
                return;
            }
            // we have some files, show last page
            offset = (int)m_currentFiles.size() - visibleSize;
            if (offset < 0)
            {
                offset = 0;
            }
            m_filesBox->SetOffset(offset);
        }

        auto sizeToProceed = std::min(maxSize - offset, visibleSize);
        visibleItems.resize(sizeToProceed);

        auto it = m_currentFiles.begin() + offset;
        auto it_end = it + sizeToProceed;
        auto vit = visibleItems.begin();
        for (; it != it_end; ++it, ++vit)
        {
            vit->text.clear();
            vit->text.push_back(it->info.fileName);
            vit->fsFlags = it->info.flags;

            if (it->info.flags & it->info.flag_uplink)
            {
                vit->openHandler = [=]() { 
                    ChangeFolder(m_currentFolderId, 
                        String(),
                        IFileSystem::queryFlags_OpenParent,
                        String(OUI_TO_STR(m_parentOffset) + OUI_STR(":") + OUI_TO_STR(m_parentPosition)));
                };
            }
            else
            if (it->info.flags & it->info.flag_directory)
            {
                vit->openHandler = [=, fileName = it->info.fileName]() {
                    ChangeFolder(m_currentFolderId,
                        fileName,
                        IFileSystem::queryFlags_OpenChild,
                        String());
                };
            }
            else
            {
                // open file here
                vit->openHandler = nullptr;
            }

            if (it->info.flags & it->info.flag_directory)
            {
                vit->colorsHandler = [=]() { return LabelColorState{ m_colorProfile->listBoxFolders, Color() }; };
            }
            else
            {
                vit->colorsHandler = nullptr;
            }
        }
        OnVisibleItemChanged();
        m_filesBox->Invalidate();
    }
    void COpenFileDialog::OnDefaultRoot(const String& name, int error)
    {
        if (error)
        {
            return;
        }
        ChangeFolder(name, String(), 0, String());
    }
    void COpenFileDialog::ChangeFolder(const FileUnifiedId& fileId, 
        const String& argument, 
        int flags,
        const String& tag)
    {
        if (m_currentOperation)
        {
            m_currentOperation->Cancel();
            m_currentOperation = nullptr;
        }
        auto operation = std::make_shared<Operation<QueryFilesHandler_type>>(
            this->GetThread(),
            std::bind(&COpenFileDialog::OnOpCompleted, this, 
                std::placeholders::_1,
                std::placeholders::_2, 
                std::placeholders::_3,
                std::placeholders::_4,
                std::placeholders::_5));

        m_currentOperation = operation;
        m_firstResult = true;

        m_fileSystem->AsyncStartQueryFiles(this->GetThread(), 
            fileId,
            argument,
            flags,
            tag,
            operation);
    }

    COpenFileDialog::COpenFileDialog(const String& rootFile, 
        FileRecipientHandler_type resultCallback,
        std::shared_ptr<IFileSystem> fileSystem)
        :
            m_resultCallback(resultCallback),
            m_fileSystem(fileSystem),
            m_rootFile(rootFile)
    {
        m_colorProfile = std::make_shared<DialogColorProfile>();
        QueryDefaultColorProfile(*m_colorProfile);

        IListBoxOwner* owner = this;
        m_filesBox = std::make_shared<CListBox>(m_colorProfile, owner);
        m_filesBox->InitColumns(2);

        m_fileEdit = std::make_shared<CEditBox>(m_colorProfile);
        m_fileLabel = std::make_shared<CLabel>(m_colorProfile, [] { return String(OUI_STR(">"));  });

        this->RegisterSwitch(m_fileEdit);
        this->RegisterSwitch(m_filesBox);
    }

    void COpenFileDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_filesBox->SetFocus();
    }
    void COpenFileDialog::ConstructChilds()
    {
        AddChild(m_filesBox);
        AddChild(m_fileEdit);
        AddChild(m_fileLabel);
        
        if (m_rootFile.native.empty())
        {
            m_fileSystem->AsyncQueryDefaultRoot(GetThread(), 
                [pthis=GetPtr_t<COpenFileDialog>(this)](const String& name, int error) 
                    { pthis->OnDefaultRoot(name, error); }
            );
        }
        else
        {
            OnDefaultRoot(m_rootFile, 0);
        }
    }
    
    void COpenFileDialog::OnVisibleItemChanged()
    {
        ListBoxItem item;
        if (!m_filesBox->GetSelectedItem(item))
        {
            return;
        }
        if (item.fsFlags & FileInfo::flag_uplink)
        {
            m_fileEdit->SetText(m_fileSystem->AppendSlash(m_currentFolderId.fullFileName));
            m_fileEdit->ScrollRight();
            m_fileEdit->Invalidate();
            return;
        }
        if (item.text.empty())
        {
            return;
        }
        auto newText = m_fileSystem->AppendSlash(m_currentFolderId.fullFileName);
        newText.native.append(item.text[0].native);
        m_fileEdit->SetText(newText);
        m_fileEdit->ScrollRight();
        m_fileEdit->Invalidate();
    }
     
    void COpenFileDialog::ShiftViewWindow(int newOffset)
    {
        const int visibleSize = m_filesBox->GetVisibleSize();
        const int totalFilesAvailable = (int)m_currentFiles.size();

        int newSelectedPositon = m_filesBox->GetSelectedPosition();

        int newSelectedOffset = newSelectedPositon + newOffset;
        int maxOffset = totalFilesAvailable - visibleSize;
        if (maxOffset < 0)
        {
            maxOffset = 0;
        }
        if (newOffset > maxOffset)
        {
            if (newSelectedOffset >= totalFilesAvailable)
            {
                auto sizeToProceed = std::min(totalFilesAvailable - maxOffset, visibleSize);
                newSelectedPositon = sizeToProceed - 1;
            }
            else
            {
                newSelectedPositon = visibleSize - (totalFilesAvailable - newSelectedOffset);
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
    int COpenFileDialog::GetTotalCount() const
    {
        return (int)m_currentFiles.size();
    }

}