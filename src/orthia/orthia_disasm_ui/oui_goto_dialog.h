#pragma once

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_editbox.h"
#include "oui_processes.h"
#include "oui_label.h"
#include "orthia_model_interfaces.h"

namespace oui
{
    struct GotoDialogInfo
    {
        orthia::GotoItem info;
        String sortKey;
        String visibleName;

        GotoDialogInfo()
        {
        }
        GotoDialogInfo(const orthia::GotoItem& item_in)
            : info(item_in)
        {
        }
    };
    inline bool operator < (const GotoDialogInfo& info1, const GotoDialogInfo& info2)
    {
        return info1.sortKey.native < info2.sortKey.native;
    }

    class CGotoDialog:public oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>, IListBoxOwner
    {
    public:

    private:
        using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>;

        std::shared_ptr<CListBox> m_listBox;
        orthia::GotoCompleteHandler_type m_resultCallback;
        std::shared_ptr<orthia::IPeristentItemStorage> m_persistentStorage;

        std::shared_ptr<CLabel> m_fileLabel;
        std::shared_ptr<CEditBox> m_fileEdit;

        bool m_firstResult = false;
        std::shared_ptr<oui::BaseOperation> m_currentOperation;

        std::vector<GotoDialogInfo> m_currentItems;
        oui::String m_currentFilter;
        int m_parentOffset = 0;
        int m_parentPosition = 0;

        const String m_openingText, m_errorText;
        std::shared_ptr<CMessageBoxWindow> m_waitBox;
        int m_openProcessSeq = 0;
        String m_waitBoxText;
        orthia::Address_type m_result = 0;
        OperationPtr_type<orthia::GotoCompleteHandler_type> m_openOperation;
        int m_typesToHighlight = 0;
        bool m_readyToExit = false;
        int m_scanFlags = 0;
        std::shared_ptr<orthia::IWorkPlaceItem> m_workPlace;

        void HighlightItem(int highlightItemOffset);

        void OnOpCompleted(std::shared_ptr<BaseOperation> operation,
            const oui::String& filter,
            const std::vector<orthia::GotoItem>& data,
            int error);
        void TryOpenAddress(orthia::Address_type address);
        void OnWaitBoxDestroyed();
        void SetOpenProcessResult(int openProcessSeq, orthia::Address_type address, int error);
        void FinishProcessOpen(std::shared_ptr<BaseOperation> op, const oui::fsui::OpenResult& result);

        template<class OwnerType, class ContainerType, class Predicate>
        friend bool DefaultShiftViewWindowToSymbol(OwnerType* owner,
            std::shared_ptr<CListBox> listBox,
            const String& symbol,
            ContainerType& container,
            Predicate predicate);
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
        CGotoDialog(const oui::CommonDialogStrings& dialogStrings,
            orthia::GotoCompleteHandler_type resultCallback,
            std::shared_ptr<orthia::IPeristentItemStorage> fileSystem,
            std::shared_ptr<orthia::IWorkPlaceItem> workPlace, 
            int scanFlags = 0);

        ~CGotoDialog();
        void ShiftViewWindow(int newPosition) override;
        bool ShiftViewWindowToSymbol(const String& symbol) override;
        int GetTotalCount() const override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };

}