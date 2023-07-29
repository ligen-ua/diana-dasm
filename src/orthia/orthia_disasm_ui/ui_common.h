#pragma once

#include "orthia_model.h"


struct UIState
{
    std::map<int, orthia::Address_type> addresses;
    std::map<int, oui::String> strings;
};

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
    void ReloadState(int itemId);
    void SaveState(int itemId);
    void SetActiveItem(int itemId);
};