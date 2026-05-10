#include "ui_common.h"
#include "oui_modal.h"
#include "orthia_expressions.h"
#include "orthia_common_print.h"

namespace oui
{

void EnumModulesByName(std::shared_ptr<orthia::IWorkPlaceItem> item, 
    const orthia::PlatformString_type& moduleName,
    std::function<bool (orthia::ModuleInfo& mod)> handler)
{
    auto moduleNameDowncased = orthia::Downcase(moduleName);

    std::vector<orthia::ModuleInfo> modules;
    item->GetModules(modules);

    orthia::PlatformString_type text;
    for (auto& mod : modules)
    {
        auto modDowncased = orthia::Downcase(mod.name);
        bool match = moduleNameDowncased == modDowncased;
        if (!match)
        {
            orthia::PlatformString_type extension;
            orthia::GetExtensionOfFile(modDowncased, &extension);
            if (!extension.empty())
            {
                modDowncased.erase(modDowncased.size() - extension.size() - 1);
                match = moduleNameDowncased == modDowncased;
            }
        }
        if (!match)
        {
            continue;
        }
        if (!handler(mod))
        {
            break;
        }
    }
}

NameResolverOverWorkplaceItem::NameResolverOverWorkplaceItem(std::shared_ptr<orthia::IWorkPlaceItem> item_in)
    :
    item(item_in)
{
}
orthia::Address_type NameResolverOverWorkplaceItem::QueryAddress(const orthia::PlatformString_type& name)
{
    auto address = item->QueryAddressByName(name, 0);
    if (!address)
    {
        address = item->QueryAddressByName(name, DI_MAX_OPERAND_SIZE);
        if (address != DI_MAX_OPERAND_SIZE)
        {
            return address;
        }
    }

    // try to find private symbols
    auto nameDowncased = orthia::Downcase(name);
    std::vector<orthia::StringInfo> parts;
    bool addressFound = false;
    orthia::SplitString(nameDowncased, orthia::StringInfo(ORTHIA_TCSTR("!")), &parts);
    if (parts.size() == 2)
    {
        auto internalName = parts[1].ToString();
        EnumModulesByName(item,
            parts[0].ToString(),
            [this, internalName, &address, &addressFound](orthia::ModuleInfo& mod)
        {

            const int c_pageSize = 5000;
            orthia::NameSelectionKey key;
            key.privateSymbolsOnly = true;
            std::vector<orthia::NameInfo> page;
            for (;;)
            {
                item->QueryNames(mod.address, key, c_pageSize, page);
                if (page.empty())
                    break;

                for (auto& name : page)
                {
                    if (orthia::Downcase(name.privateSymbol.native) == internalName)
                    {
                        address = name.address;
                        addressFound = true;
                        return false;
                    }
                }
                key.flags |= key.flags_ContinueFrom;
                key.address = page.back().address;
                key.continueMarkNameFlag = page.back().flags;
            }
            return true;
        });

        if (addressFound)
        {
            return address;
        }
    }
    throw std::runtime_error("Unknown variable: " + orthia::PlatformStringToUtf8(name));
}
orthia::Address_type NameResolverOverWorkplaceItem::Dereference(orthia::Address_type address) 
{
    auto res = item->ReadData(address, item->GetDianaMode());
    if (res.rangeFlags & res.flags_FullValid)
    {
        return Diana_ReadValue(res.pDataStart, item->GetDianaMode());
    }
    throw std::runtime_error("Can't dereference: " + orthia::PlatformStringToUtf8(orthia::AddressToString(address, item->GetDianaMode())));
}

}
// CUIStateManager
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
    auto baseDialogNode = g_textManager->QueryNodeDef(ORTHIA_TCSTR("ui.dialog.basedialog"));
    auto dialogNode = g_textManager->QueryNodeDef(dialog.native);
    Load(ORTHIA_TCSTR("caption"), strs.caption, baseDialogNode, dialogNode);
    Load(ORTHIA_TCSTR("opening"), strs.openingText, baseDialogNode, dialogNode);
    Load(ORTHIA_TCSTR("error"), strs.errorText, baseDialogNode, dialogNode);
    Load(ORTHIA_TCSTR("ok"), strs.okText, baseDialogNode, dialogNode);
    Load(ORTHIA_TCSTR("cancel"), strs.cancelText, baseDialogNode, dialogNode);
}


namespace oui
{

    orthia::Address_type CaptureAddress(const orthia::PlatformString_type& addressString_in)
    {
        orthia::PlatformString_type addressString = addressString_in;
        orthia::TrimStringAllWhiteSpace(addressString);

        if (addressString.empty())
        {
            throw std::runtime_error("Invalid argument");
        }
        orthia::Address_type address = 0;
        if (addressString.size() > 2 && addressString[0] == OUI_TCHAR('0') && addressString[1] == OUI_TCHAR('x'))
        {
            orthia::HexStringToObject(orthia::PlatformString_type(addressString.begin() + 2, addressString.end()), &address);
            return address;
        }
        if (addressString.size() > 2 && addressString[0] == OUI_TCHAR('0') && addressString[1] == OUI_TCHAR('n'))
        {
            orthia::StringToObject(orthia::PlatformString_type(addressString.begin() + 2, addressString.end()), &address);
            return address;
        }
        if (addressString.back() == OUI_TCHAR('h'))
        {
            // asm format
            orthia::HexStringToObject(orthia::PlatformString_type(addressString.begin(), addressString.end() - 1), &address);
            return address;
        }
        if (addressString.find(OUI_TCHAR('`')) != addressString.npos)
        {
            // windbg format
            auto copy = addressString;
            copy.erase(std::remove(copy.begin(), copy.end(), OUI_TCHAR('`')), copy.end());
            orthia::HexStringToObject(copy, &address);
            return address;
        }
        orthia::HexStringToObject(addressString, &address);
        return address;
    }

    orthia::Address_type CaptureAddressExp(const orthia::PlatformString_type& expression, std::shared_ptr<orthia::IWorkPlaceItem> item)
    {
        auto resolver = std::make_shared< NameResolverOverWorkplaceItem>(item);
        return orthia::CaptureAddressExp(expression, resolver);
    }
}