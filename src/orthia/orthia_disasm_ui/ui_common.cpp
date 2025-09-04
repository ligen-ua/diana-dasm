#include "ui_common.h"
#include "oui_modal.h"

void CUIStateManager::Register(std::shared_ptr<IUIStatefulWindow> window)
{
    m_windows.insert(window);
}
void CUIStateManager::SetActiveItem(int itemId)
{
    for (auto& wnd : m_windows)
    {
        wnd->SetActiveWorkspaceItem(itemId);
    }
}
void CUIStateManager::ReloadState(int itemId)
{
    auto item = m_workspaces.find(itemId);
    if (item == m_workspaces.end())
    {
        // no data
        return;
    }
    auto & workItem = item->second;
    for (auto& state : workItem.m_states)
    {
        state.first->ReloadState(state.second);
    }
}

void CUIStateManager::SaveState(int itemId)
{
    auto& workItem = m_workspaces[itemId];
    for (auto& wnd : m_windows)
    {
        workItem.m_states.insert(std::make_pair(wnd, UIState()));
    }
    for (auto& state : workItem.m_states)
    {
        state.first->SaveState(state.second);
    }
}

template<class NodePtr>
void Load(const orthia::PlatformString_type & id, oui::String& value, NodePtr base, NodePtr derived)
{
    value = derived->QueryValueDef(id, orthia::PlatformString_type());
    if (value.native.empty())
    {
        value = base->QueryValue(id);
    }
}

void GetCommonDialogStrings(const oui::String& dialog, oui::CommonDialogStrings& strs)
{
    auto baseDialogNode = g_textManager->QueryNodeDef(L"ui.dialog.basedialog");
    auto dialogNode = g_textManager->QueryNodeDef(dialog.native);
    Load(L"caption", strs.caption, baseDialogNode, dialogNode);
    Load(L"opening", strs.openingText, baseDialogNode, dialogNode);
    Load(L"error", strs.errorText, baseDialogNode, dialogNode);
    Load(L"ok", strs.okText, baseDialogNode, dialogNode);
    Load(L"cancel", strs.cancelText, baseDialogNode, dialogNode);
}