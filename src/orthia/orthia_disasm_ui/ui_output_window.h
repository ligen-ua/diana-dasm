#pragma once

#include "oui_containers.h"
#include "oui_multiline_view.h"
#include "orthia_model.h"

class COutputWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner, public orthia::IUILogInterface
{
    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void CancelAllQueries() override;
    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

    // orthia::IUILogInterface
    void WriteLog(const oui::String& line) override;
public:
    COutputWindow(std::function<oui::String()> getCaption);
    void AddLine(const oui::String& line);
    bool ScrollUp(oui::MultiLineViewItem* item, int count) override;
    bool ScrollDown(oui::MultiLineViewItem* item, int count) override;
};