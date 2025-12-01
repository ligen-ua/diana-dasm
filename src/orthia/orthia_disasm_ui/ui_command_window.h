#pragma once

#include "oui_containers.h"
#include "oui_multiline_view.h"
#include "orthia_model.h"
#include "ui_common.h"
#include "oui_editbox.h"
#include "oui_label.h"

class CCommandWindow:public oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>, public IUIStatefulWindow, oui::CSelfHostedMultiLineViewOwner
{
    using Parent_type = oui::ChildSwitcher<oui::SimpleBrush<oui::CPanelWindow>, oui::IPanelChildSwitcher>;

    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;
    std::shared_ptr<oui::CEditBox> m_commandEdit;
    std::shared_ptr<oui::CLabel> m_textLabel;

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    std::shared_ptr<oui::CMultiLineView> SF_GetView() override;
    oui::CConsole* SF_GetConsole() override;

    void AddLine(const oui::String& line); 
    void OnAfterInit(std::shared_ptr<oui::CWindowsPool> pool) override;

public:
    CCommandWindow(std::function<oui::String()> getCaption,
                   std::shared_ptr<orthia::CProgramModel> model,
                   std::shared_ptr<oui::IPanelChildSwitcher> parentTabSwitcher);

    void WriteLog(const oui::String& text);

    void OnWorkspaceItemChanged();
    void SetActiveWorkspaceItem(int itemId);
    void ReloadState(const UIState& state);
    void SaveState(UIState& state);
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext);


};
