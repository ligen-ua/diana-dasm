#pragma once

#include "oui_containers.h"
#include "oui_multiline_view.h"
#include "orthia_model.h"

class COutputWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::CSelfHostedMultiLineViewOwner, public orthia::IUILogInterface
{
    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;
    void OnContextMenu(const oui::Point& point) override;

    // orthia::IUILogInterface
    void WriteLog(const oui::String& line) override;

    std::shared_ptr<oui::CMultiLineView> SF_GetView() override;
    oui::CConsole* SF_GetConsole() override;
public:
    COutputWindow(std::function<oui::String()> getCaption);
    void AddLine(const oui::String& line);
};

void RegisterAsLog(std::shared_ptr<COutputWindow> outputWindow);
