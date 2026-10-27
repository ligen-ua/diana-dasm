#define  _CRT_SECURE_NO_WARNINGS
#include "oui_open_saved_dialog.h"
#include "ui_common.h"

namespace oui
{
    namespace
    {
        // newest first
        String MakeSortKey(const orthia::DatabaseInfo& info)
        {
            const auto ticks = (unsigned long long)info.lastWrite.time_since_epoch().count();
            return String(orthia::ToWideStringAsHex(~ticks) + OUI_TCSTR("@") + info.folderName);
        }

        String Notes(const orthia::DatabaseInfo& info)
        {
            String::string_type text;
            auto add = [&](const ORTHIA_TCHAR* note) {
                if (!text.empty())
                    text += OUI_TCSTR(", ");
                text += note;
            };
            if (info.hasCapture)
                add(OUI_TCSTR("captured"));
            if (info.hasFloatingSnapshots)
                add(OUI_TCSTR("floating code saved"));
            if (!info.hasDataDb)
                add(OUI_TCSTR("no data.db"));
            return String(text);
        }
    }

    void COpenSavedProcessDialog::OnResize()
    {
        const auto clientRect = GetClientRect();
        if (clientRect.size.width < 5 || clientRect.size.height < 5)
        {
            Size zeroSize;
            m_listBoxScrollable->Resize(zeroSize);
            m_edit->Resize(zeroSize);
            m_editLabel->Resize(zeroSize);
            return;
        }

        Rect boxRect = clientRect;
        boxRect.position.x += 2;
        boxRect.position.y += 4;
        boxRect.size.width -= 4;
        boxRect.size.height -= 4;
        if (m_listBox->GetSize().width < boxRect.size.width)
        {
            m_listBox->Resize(boxRect.size);
        }
        m_listBoxScrollable->MoveTo(boxRect.position);
        m_listBoxScrollable->Resize(boxRect.size);

        Rect editRect = clientRect;
        editRect.position.x += 3;
        editRect.position.y += 2;
        editRect.size.width -= 5;
        editRect.size.height = 1;
        m_edit->MoveTo(editRect.position);
        m_edit->Resize(editRect.size);

        Rect labelRect = editRect;
        --labelRect.position.x;
        labelRect.size.width = 1;
        labelRect.size.height = 1;
        m_editLabel->MoveTo(labelRect.position);
        m_editLabel->Resize(labelRect.size);

        Parent_type::OnResize();
        // the rows that fit now
        UpdateVisibleItems();
    }

    void COpenSavedProcessDialog::UpdateVisibleItems()
    {
        DefaultUpdateVisibleItems(this, this, m_listBox, m_items,
            [&](auto it, auto vit)
        {
            vit->text.clear();
            vit->text.push_back(it->visibleName);
            vit->text.push_back(String(orthia::FormatFileTime(it->info.lastWrite)));
            vit->text.push_back(String(orthia::FormatSize(it->info.sizeBytes)));
            vit->text.push_back(Notes(it->info));
            vit->openHandler = [=, folder = it->info.folder]() {
                Open(folder);
            };
            vit->colorsHandler = [=]() { return LabelColorState{ m_colorProfile->listBoxFolders, Color() }; };
        });
    }

    void COpenSavedProcessDialog::Open(const orthia::PlatformString_type& argument)
    {
        if (argument.empty() || !m_openHandler)
        {
            return;
        }
        auto result = m_openHandler(argument);
        if (!result.error.native.empty())
        {
            ShowError(result.error);
            return;
        }
        m_openHandler = nullptr;
        FinishDialog();
    }

    void COpenSavedProcessDialog::ShowError(const String& text)
    {
        auto box = AddChildAndInit_t(std::make_shared<CMessageBoxWindow>(
            [=]() { return text; },
            [=]() {}));
        box->Dock();
    }

    COpenSavedProcessDialog::COpenSavedProcessDialog(const oui::CommonDialogStrings& dialogStrings,
        QueryFolders_type queryFolders,
        OpenHandler_type openHandler)
        :
        m_openHandler(std::move(openHandler)),
        m_queryFolders(std::move(queryFolders))
    {
        SetCaption(dialogStrings.caption);

        IListBoxOwner* owner = this;
        m_listBox = std::make_shared<CListBox>(m_colorProfile, owner);
        m_listBoxScrollable = std::make_shared<oui::CScrollable>(m_listBox);
        auto columnsNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.opensaved.columns"));
        m_listBox->InitColumns(
            oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("process")); }, 50),
            oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("modified")); }, 17),
            oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("size")); }, 10),
            oui::ColumnParam([=] { return columnsNode->QueryValue(ORTHIA_TCSTR("notes")); }, 23));
        m_listBox->Dock();

        // a path, a pid or a folder name: what --proc-folder takes
        m_edit = std::make_shared<CEditBox>(m_colorProfile);
        m_edit->SetEnterHandler([this](const String& text) {
            auto argument = text.native;
            orthia::TrimStringAllWhiteSpace(argument);
            Open(argument);
        });
        m_edit->SetSelectAllOnFocus(true);

        auto labelProfile = std::shared_ptr<LabelColorProfile>(m_colorProfile, &m_colorProfile->label);
        m_editLabel = std::make_shared<CLabel>(labelProfile, [] { return String(OUI_STR(">")); });

        this->RegisterSwitch(m_edit);
        this->RegisterSwitch(m_listBox);
    }

    void COpenSavedProcessDialog::OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool)
    {
        m_items.clear();
        if (m_queryFolders)
        {
            for (auto& info : m_queryFolders())
            {
                SavedFolderDialogInfo item;
                item.visibleName = String(info.folderName);
                item.sortKey = MakeSortKey(info);
                item.info = std::move(info);
                if (auto console = GetConsole())
                {
                    console->FilterOrReplaceUnreadableSymbols(item.visibleName);
                }
                m_items.push_back(std::move(item));
            }
        }
        std::sort(m_items.begin(), m_items.end());
        m_listBox->SetOffset(0);
        m_listBox->SetSelectedPosition(0);
        UpdateVisibleItems();
        if (m_items.empty())
        {
            m_edit->SetFocus();
        }
        else
        {
            m_listBox->SetFocus();
            OnVisibleItemChanged();
        }
    }

    void COpenSavedProcessDialog::ConstructChilds()
    {
        AddChild(m_edit);
        AddChild(m_editLabel);
        AddChild(m_listBoxScrollable);
    }

    void COpenSavedProcessDialog::OnVisibleItemChanged()
    {
        const int index = m_listBox->GetOffset() + m_listBox->GetSelectedPosition();
        if (index < 0 || index >= (int)m_items.size())
        {
            return;
        }
        m_edit->SetText(String(m_items[index].info.folder));
        m_edit->ScrollRight();
        m_edit->Invalidate();
    }

    void COpenSavedProcessDialog::HighlightItem(int highlightItemOffset)
    {
        DefaultHighlightItem(m_listBox, highlightItemOffset, m_items.size());
    }

    bool COpenSavedProcessDialog::ShiftViewWindowToSymbol(const String& symbol)
    {
        return DefaultShiftViewWindowToSymbol(this, m_listBox, symbol, m_items,
            [](const SavedFolderDialogInfo& info, const String& symbol) {
                return StartsWith(info.visibleName.native, symbol.native);
            });
    }

    void COpenSavedProcessDialog::ShiftViewWindow(int newOffset)
    {
        DefaultShiftViewWindow(m_listBox, newOffset, m_items.size());
        UpdateVisibleItems();
    }

    int COpenSavedProcessDialog::GetTotalCount() const
    {
        return (int)m_items.size();
    }

    bool COpenSavedProcessDialog::ProcessEvent(InputEvent& evt, WindowEventContext& evtContext)
    {
        // the arrow goes from the edit box to the list
        if (evt.keyEvent.valid && evt.keyEvent.virtualKey == VirtualKey::Down && m_edit->IsFocused() && !m_items.empty())
        {
            m_listBox->SetFocus();
            return true;
        }
        return Parent_type::ProcessEvent(evt, evtContext);
    }
}
