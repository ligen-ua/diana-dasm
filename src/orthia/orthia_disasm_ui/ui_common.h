#pragma once

#include "orthia_model.h"
#include "orthia_expressions.h"

struct UIState
{
    std::map<int, orthia::Address_type> addresses;
    std::map<int, oui::String> strings;
};

template<class Container, class Key, class Handler, class Handler2>
void Apply(Container& container, Key key, Handler handler, Handler2 handler2)
{
    auto it = container.find(key);
    if (it == container.end())
    {
        handler2();
        return;
    }
    handler(it->second);
}

struct IUIStatefulWindow
{
    virtual ~IUIStatefulWindow() {}
    virtual void ReloadState(const UIState& state) {}
    virtual void SaveState(UIState& state) {}
    virtual void SetActiveWorkspaceItem(int itemId) = 0;
};

struct UIWorkspaceState
{
    using StatesMap_type = std::map<std::shared_ptr<IUIStatefulWindow>, UIState>;
    StatesMap_type m_states;
};
class CUIStateManager
{
    using WorkspaceStateMap_type = std::map<int, UIWorkspaceState>;
    WorkspaceStateMap_type m_workspaces;

    std::set<std::shared_ptr<IUIStatefulWindow>> m_windows;
public:
    void Register(std::shared_ptr<IUIStatefulWindow> window);
    bool ReloadState(int itemId);
    void SaveState(int itemId);
    void SetActiveItem(int itemId);
    UIState * GetUIState(int itemId, std::shared_ptr<IUIStatefulWindow> window);
};

namespace oui
{
    struct CommonDialogStrings;
}
void GetCommonDialogStrings(const oui::String& dialog, oui::CommonDialogStrings& strs);

namespace oui
{

    struct NameResolverOverWorkplaceItem :orthia::INameResolver
    {
        std::shared_ptr<orthia::IWorkPlaceItem> item;
        NameResolverOverWorkplaceItem(std::shared_ptr<orthia::IWorkPlaceItem> item_in);
        orthia::Address_type QueryAddress(const orthia::PlatformString_type& name) override;
        orthia::Address_type Dereference(orthia::Address_type address) override;
    };

    orthia::Address_type CaptureAddress(const orthia::PlatformString_type& addressString);
    orthia::Address_type CaptureAddressExp(const orthia::PlatformString_type& expression, std::shared_ptr<orthia::IWorkPlaceItem> item);

    void EnumModulesByName(std::shared_ptr<orthia::IWorkPlaceItem> item,
        const orthia::PlatformString_type& moduleName,
        std::function<bool(orthia::ModuleInfo& mod)> handler);
}