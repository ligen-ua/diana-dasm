#pragma once

// File > Open Saved Process: the process folders of the data folder, newest first. Enter on one, or a
// folder path, a pid or a folder name typed into the edit box, opens it as --proc-folder does

#include "oui_modal.h"
#include "oui_listbox.h"
#include "oui_editbox.h"
#include "oui_label.h"
#include "oui_scrollable.h"
#include "oui_processes.h"
#include "orthia_databases.h"

namespace oui
{
    struct SavedFolderDialogInfo
    {
        orthia::DatabaseInfo info;
        String sortKey;
        String visibleName;
    };
    inline bool operator < (const SavedFolderDialogInfo& info1, const SavedFolderDialogInfo& info2)
    {
        return info1.sortKey.native < info2.sortKey.native;
    }

    class COpenSavedProcessDialog :public oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>, IListBoxOwner
    {
    public:
        // what the user picked: a folder, or the text typed; a non-empty error keeps the dialog open
        using OpenHandler_type = std::function<fsui::OpenResult(const orthia::PlatformString_type& argument)>;
        // the folders to list, read when the dialog opens
        using QueryFolders_type = std::function<std::vector<orthia::DatabaseInfo>()>;

    private:
        using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<CModalWindow>>;

        std::shared_ptr<CListBox> m_listBox;
        std::shared_ptr<oui::CScrollable> m_listBoxScrollable;
        std::shared_ptr<CLabel> m_editLabel;
        std::shared_ptr<CEditBox> m_edit;
        OpenHandler_type m_openHandler;
        QueryFolders_type m_queryFolders;
        std::vector<SavedFolderDialogInfo> m_items;

        void Open(const orthia::PlatformString_type& argument);
        void HighlightItem(int highlightItemOffset);
        void ShowError(const String& text);

        template<class OwnerType, class ContainerType, class Predicate>
        friend bool DefaultShiftViewWindowToSymbol(OwnerType* owner,
            std::shared_ptr<CListBox> listBox,
            const String& symbol,
            ContainerType& container,
            Predicate predicate);
    protected:
        void OnResize() override;
        void ConstructChilds() override;
        void UpdateVisibleItems();
        void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;
        void OnVisibleItemChanged() override;
        // the list is read at once: nothing runs in the background
        void CancelAllQueries() override {}

    public:
        COpenSavedProcessDialog(const oui::CommonDialogStrings& dialogStrings,
            QueryFolders_type queryFolders,
            OpenHandler_type openHandler);

        void ShiftViewWindow(int newPosition) override;
        bool ShiftViewWindowToSymbol(const String& symbol) override;
        int GetTotalCount() const override;
        bool ProcessEvent(InputEvent& evt, WindowEventContext& evtContext) override;
    };
}
