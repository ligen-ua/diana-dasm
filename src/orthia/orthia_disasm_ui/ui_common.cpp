#include "ui_common.h"

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