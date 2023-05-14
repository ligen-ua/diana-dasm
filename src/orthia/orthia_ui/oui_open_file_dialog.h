#pragma once

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_editbox.h"
#include "oui_filesystem.h"
#include "oui_label.h"

namespace oui
{
    struct FileDialogInfo
    {
        FileInfo info;
        String sortKey;
        String visibleName;

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

    class COpenFileDialog:public oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>, IListBoxOwner
    {
    public:
        using FileRecipientHandler_type = std::function<fsui::OpenResult(std::shared_ptr<COpenFileDialog>, std::shared_ptr<IFile>, OperationPtr_type<fsui::FileCompleteHandler_type>)>;

    private:
        using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>;

        std::shared_ptr<CListBox> m_filesBox;
        FileRecipientHandler_type m_resultCallback;
        std::shared_ptr<IFileSystem> m_fileSystem;
        const String m_rootFile;

        std::shared_ptr<CLabel> m_fileLabel;
        std::shared_ptr<CEditBox> m_fileEdit;

        bool m_firstResult = false;
        OperationPtr_type<QueryFilesHandler_type> m_currentOperation;
        std::vector<FileDialogInfo> m_currentFiles;
        FileUnifiedId m_currentFolderId;
        int m_parentOffset = 0;
        int m_parentPosition = 0;

        const String m_openingText, m_errorText;
        std::shared_ptr<CMessageBoxWindow> m_waitBox;
        int m_openFileSeq = 0;
        String m_waitBoxText;
        std::shared_ptr<IFile> m_result;
        OperationPtr_type<oui::fsui::FileCompleteHandler_type> m_openOperation;
        int m_typesToHighlight = 0;
        bool m_readyToExit = false;

        void OnOpCompleted(std::shared_ptr<BaseOperation> operation,
            const FileUnifiedId& folderId,
            const std::vector<FileInfo>& data,
            int error,
            const String& tag);
        void ChangeFolder(const FileUnifiedId& fileId,
            const String& argument,
            int flags,
            const String& tag);
        void HighlightItem(int highlightItemOffset);
        void TryOpenFile(const FileUnifiedId& folderId,
            const String& fileName,
            bool combine);
        void OnWaitBoxDestroyed();
        void SetOpenFileResult(int openFileSeq, std::shared_ptr<IFile> file, int error, const String& folderName);
        void FinishFileOpen(std::shared_ptr<BaseOperation> op, const oui::fsui::OpenResult& result);
    protected:
        void OnResize() override;
        void AsyncQuery(CListBox* listBox, std::function<void(const ListBoxItem*, int)> handler, int offset, int size);

        void CancelAllQueries() override;

        void ConstructChilds() override;
        void OnDefaultRoot(const String& name, int error);
        void UpdateVisibleItems();
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;

        void OnFinishDialog() override;
        void OnVisibleItemChanged() override;
        String GetWaitBoxText();

    public:
        COpenFileDialog(const String& rootFile,
            const String& openingText,
            const String& errorText,
            FileRecipientHandler_type resultCallback,
            std::shared_ptr<IFileSystem> fileSystem,
            int typesToHighlight = 0);
        ~COpenFileDialog();
        void ShiftViewWindow(int newPosition) override;
        bool ShiftViewWindowToSymbol(const String& symbol) override;
        int GetTotalCount() const override;
    };

}