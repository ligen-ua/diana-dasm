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
            return;
        }

        Rect boxRect = clientRect;
        boxRect.position.x += 2;
        boxRect.position.y += 4;
        boxRect.size.width -= 4;
        boxRect.size.height -= 4;

        m_filesBox->MoveTo(boxRect.position);
        m_filesBox->Resize(boxRect.size);

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
        int error)
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

            if (!m_currentFolderId.fullFileName.native.empty())
            {
                FileInfo info;
                info.fileName = OUI_STR("..");
                info.flags |= info.flag_directory | info.flag_uplink;
                m_currentFiles.push_back(info);
            }
            m_filesBox->SetSelectedPosition(0);
        }
        else
        {
            m_currentFiles.reserve(m_currentFiles.size() + data.size());
        }


        String highlighName;
        for (auto& info : data)
        {
            m_currentFiles.push_back(info);
            GenSortKey(m_currentFiles.back());

            if (info.flags & info.flag_highlight)
            {
                highlighName = info.fileName;
            }
        }

        std::sort(m_currentFiles.begin(), m_currentFiles.end());

        if (!highlighName.native.empty())
        {
            // TODO: improve selection
            auto it = std::find_if(m_currentFiles.begin(), m_currentFiles.end(), [&](auto& value) { return value.info.fileName.native == highlighName.native;  });
            size_t offset = it - m_currentFiles.begin();
            m_filesBox->SetOffset((int)offset);
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
            if (it->info.flags & it->info.flag_uplink)
            {
                vit->openHandler = [=]() { 
                    ChangeFolder(m_currentFolderId, 
                        String(),
                        IFileSystem::queryFlags_OpenParent);  
                };
            }
            else
            {
                vit->openHandler = [=, fileName = it->info.fileName]() {
                    ChangeFolder(m_currentFolderId,
                        fileName,
                        IFileSystem::queryFlags_OpenChild);
                };
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
        m_filesBox->Invalidate();
    }
    void COpenFileDialog::OnDefaultRoot(const String& name, int error)
    {
        if (error)
        {
            return;
        }
        ChangeFolder(name, String(), 0);
    }
    void COpenFileDialog::ChangeFolder(const FileUnifiedId& fileId, const String& argument, int flags)
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
                std::placeholders::_4));

        m_currentOperation = operation;
        m_firstResult = true;

        m_fileSystem->AsyncStartQueryFiles(this->GetThread(), fileId, argument, flags, operation);
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
    }

    void COpenFileDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_filesBox->SetFocus();
    }
    void COpenFileDialog::ConstructChilds()
    {
        AddChild(m_filesBox);

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
    void COpenFileDialog::ShiftViewWindow(int newOffset)
    {
        const int visibleSize = m_filesBox->GetVisibleSize();
        const int totalFilesAvailable = (int)m_currentFiles.size();
        int newSelectedPositon = m_filesBox->GetSelectedPosition();

        int maxOffset = totalFilesAvailable - visibleSize;
        if (newOffset >= maxOffset)
        {
            if (visibleSize && (newOffset + newSelectedPositon) >= maxOffset)
            {
                newSelectedPositon = visibleSize - 1;
            }
            newOffset = maxOffset;
        }
        if (newOffset < 0)
        {
            newOffset = 0;
            newSelectedPositon = 0;
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