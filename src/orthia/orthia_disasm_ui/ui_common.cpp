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
UIState* CUIStateManager::GetUIState(int itemId, std::shared_ptr<IUIStatefulWindow> window)
{
    auto item = m_workspaces.find(itemId);
    if (item == m_workspaces.end())
    {
        // no data
        return 0;
    }
    auto wit = item->second.m_states.find(window);
    if (wit == item->second.m_states.end())
    {
        // no data
        return 0;
    }
    return &wit->second;
}

bool CUIStateManager::ReloadState(int itemId)
{
    auto pair = m_workspaces.insert(std::make_pair(itemId, UIWorkspaceState()));
    auto item = pair.first;
    auto & workItem = item->second;

    if (pair.second)
    {
        // new item
        for (auto& wnd : m_windows)
        {
            workItem.m_states.insert(std::make_pair(wnd, UIState()));
        }
    }
    for (auto& state : workItem.m_states)
    {
        state.first->ReloadState(state.second);
    }
    return pair.second;
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


namespace oui
{

    orthia::Address_type CaptureAddress(const std::wstring& addressString)
    {
        if (addressString.empty())
        {
            throw std::runtime_error("Invalid argument");
        }
        ULONGLONG address = 0;
        if (addressString.size() > 2 && addressString[0] == '0' && addressString[1] == 'x')
        {
            orthia::HexStringToObject(std::wstring(addressString.begin() + 2, addressString.end()), &address);
            return address;
        }
        if (addressString.size() > 2 && addressString[0] == '0' && addressString[1] == 'n')
        {
            orthia::StringToObject(std::wstring(addressString.begin() + 2, addressString.end()), &address);
            return address;
        }
        if (addressString.back() == 'h') 
        {  
            // asm format
            orthia::HexStringToObject(std::wstring(addressString.begin(), addressString.end()-1), &address);
            return address;
        }
        if (addressString.find('`') != addressString.npos)
        {
            // windbg format
            auto copy = addressString;
            copy.erase(std::remove(copy.begin(), copy.end(), '`'), copy.end());
            orthia::HexStringToObject(copy, &address);
            return address;
        }
        orthia::HexStringToObject(addressString, &address);
        return address;
    }


}