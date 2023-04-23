#pragma once

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_filesystem.h"

namespace oui
{
    struct FileDialogInfo
    {
        FileInfo info;
        String sortKey;

        FileDialogInfo()
        {
        }
        FileDialogInfo(const FileInfo& info_in)
            :   info(info_in)
        {
        }
    };
    inline bool operator < (const FileDialogInfo& info1, const FileDialogInfo& info2)
    {
        return info1.sortKey.native < info2.sortKey.native;
    }

    class COpenFileDialog:public oui::SimpleBrush<CModalWindow>, IListBoxOwner
    {
        using Parent_type = oui::SimpleBrush<CModalWindow>;
        std::shared_ptr<DialogColorProfile> m_colorProfile;

        std::shared_ptr<CListBox> m_filesBox;
        FileRecipientHandler_type m_resultCallback;
        std::shared_ptr<IFileSystem> m_fileSystem;
        const String m_rootFile;

        bool m_firstResult = false;
        OperationPtr_type<QueryFilesHandler_type> m_currentOperation;
        std::vector<FileDialogInfo> m_currentFiles;
        FileUnifiedId m_currentFolderId;

        void OnOpCompleted(std::shared_ptr<BaseOperation> operation,
            const FileUnifiedId& folderId,
            const std::vector<FileInfo>& data,
            int error);
        void ChangeFolder(const String& name);

    protected:
        void OnResize() override;
        void AsyncQuery(CListBox* listBox, std::function<void(const ListBoxItem*, int)> handler, int offset, int size);

        void CancelAllQueries() override;

        void ConstructChilds() override;
        void OnDefaultRoot(const String& name, int error);
        void UpdateVisibleItems();
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;

    public:
        COpenFileDialog(const String& rootFile,
            FileRecipientHandler_type resultCallback,
            std::shared_ptr<IFileSystem> fileSystem);

        void ShiftViewWindow(int newPosition) override;
        int GetTotalCount() const override;
    };

}