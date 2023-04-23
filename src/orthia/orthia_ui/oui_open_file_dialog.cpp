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

            FileInfo info;
            info.fileName = OUI_STR("..");
            info.flags |= info.flag_directory;
            m_currentFiles.push_back(info);
        }
        else
        {
            m_currentFiles.reserve(m_currentFiles.size() + data.size());
        }

        for (auto& info : data)
        {
            m_currentFiles.push_back(info);
            GenSortKey(m_currentFiles.back());
        }

        std::sort(m_currentFiles.begin(), m_currentFiles.end());

        // update visible items
        const auto offset = m_filesBox->GetOffset();
        const auto visibleSize = m_filesBox->GetVisibleSize();
        auto& visibleItems = m_filesBox->GetItems();
        const int maxSize = (int)m_currentFiles.size();
        if (offset >= maxSize)
        {
            visibleItems.clear();
            m_filesBox->Invalidate();
            return;
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
        }
        m_filesBox->Invalidate();
    }
    void COpenFileDialog::OnDefaultRoot(const String& name, int error)
    {
        if (error)
        {
            return;
        }
        ChangeFolder(name);
    }
    void COpenFileDialog::ChangeFolder(const String& name)
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

        m_currentFiles.clear();
        m_currentOperation = operation;

        m_fileSystem->AsyncStartQueryFiles(this->GetThread(), name, operation);
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
}