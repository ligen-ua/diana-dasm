#pragma once

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_editbox.h"
#include "oui_processes.h"
#include "oui_label.h"

namespace oui
{
    struct ProcessDialogInfo
    {
        ProcessInfo info;
        String sortKey;
        String visibleName;

        ProcessDialogInfo()
        {
        }
        ProcessDialogInfo(const ProcessInfo& info_in)
            :   info(info_in)
        {
        }
    };
    inline bool operator < (const ProcessDialogInfo& info1, const ProcessDialogInfo& info2)
    {
        return info1.sortKey.native < info2.sortKey.native;
    }

    class COpenProcessDialog:public oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>, IListBoxOwner
    {
    public:
        using ProcessRecipientHandler_type = std::function<fsui::OpenResult(std::shared_ptr<COpenProcessDialog>, std::shared_ptr<IProcess>, OperationPtr_type<fsui::ProcessCompleteHandler_type>)>;

    private:
        using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>;

        std::shared_ptr<CListBox> m_filesBox;
        ProcessRecipientHandler_type m_resultCallback;
        std::shared_ptr<IProcessSystem> m_fileSystem;

        std::shared_ptr<CLabel> m_fileLabel;
        std::shared_ptr<CEditBox> m_fileEdit;

        bool m_firstResult = false;
        OperationPtr_type<QueryProcessHandler_type> m_currentOperation;
        std::vector<ProcessDialogInfo> m_currentProcess;
        ProcessUnifiedId m_currentFolderId;
        int m_parentOffset = 0;
        int m_parentPosition = 0;

        const String m_openingText, m_errorText;
        std::shared_ptr<CMessageBoxWindow> m_waitBox;
        int m_openProcessSeq = 0;
        String m_waitBoxText;
        std::shared_ptr<IProcess> m_result;
        OperationPtr_type<oui::fsui::ProcessCompleteHandler_type> m_openOperation;
        int m_typesToHighlight = 0;
        bool m_readyToExit = false;
        void HighlightItem(int highlightItemOffset);

        void OnOpCompleted(std::shared_ptr<BaseOperation> operation,
            const ProcessUnifiedId& folderId,
            const std::vector<ProcessInfo>& data,
            int error);
        void TryOpenProcess(const ProcessUnifiedId& folderId);
        void OnWaitBoxDestroyed();
        void SetOpenProcessResult(int openProcessSeq, std::shared_ptr<IProcess> file, int error);
        void FinishProcessOpen(std::shared_ptr<BaseOperation> op, const oui::fsui::OpenResult& result);
    protected:
        void OnResize() override;
        void CancelAllQueries() override;

        void ConstructChilds() override;
        void UpdateVisibleItems();
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;

        void OnFinishDialog() override;
        void OnVisibleItemChanged() override;
        String GetWaitBoxText();

    public:
        COpenProcessDialog(const String& openingText,
            const String& errorText,
            ProcessRecipientHandler_type resultCallback,
            std::shared_ptr<IProcessSystem> fileSystem);
        ~COpenProcessDialog();
        void ShiftViewWindow(int newPosition) override;
        bool ShiftViewWindowToSymbol(const String& symbol) override;
        int GetTotalCount() const override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };

}