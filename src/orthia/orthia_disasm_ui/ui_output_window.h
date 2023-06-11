#pragma once

#include "oui_containers.h"
#include "oui_multiline_view.h"

class COutputWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner
{
    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void CancelAllQueries() override;
    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;

public:
    COutputWindow(std::function<oui::String()> getCaption);
    void AddLine(const oui::String& line);
};