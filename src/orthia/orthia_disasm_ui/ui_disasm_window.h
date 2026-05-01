#pragma once
#include "oui_containers.h"
#include "oui_multiline_view.h"
#include "ui_common.h"

class CDisasmWindow:public oui::SimpleBrush<oui::CPanelWindow>, oui::IMultiLineViewOwner, public IUIStatefulWindow
{
    struct ReloadVisibleDataContext
    {
        bool scrollUp = false;
        ReloadVisibleDataContext()
        {
        }
    };
    static const int field_peAddress_index = 1;
    static const int field_peAddress_subIndex = 2;

    using Parent_type = oui::SimpleBrush<oui::CPanelWindow>;

    std::shared_ptr<orthia::CProgramModel> m_model;

    // these vars form a content iterator
    int m_metaInfoPos = 0;
    oui::LineIndex m_peAddress;

    int m_itemUid = -1;

    std::shared_ptr<oui::CMultiLineView> m_view;
    std::shared_ptr<oui::DialogColorProfile> m_colorProfile;

    void CancelAllQueries() override;
    bool ScrollUp(oui::MultiLineViewItem* item, int count) override;
    bool ScrollDown(oui::MultiLineViewItem* item, int count) override;
    void ReloadVisibleData(const ReloadVisibleDataContext& context = ReloadVisibleDataContext());

    void ConstructChilds() override;
    void OnResize() override;
    void SetFocusImpl() override;
    void SetActiveItemImpl(int itemUid);
    bool ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext) override;
    void Event_Goto(int scanFlags = 0);
    void Event_XrefDialog(orthia::Address_type targetAddress);
    void DoGoto(orthia::Address_type address, orthia::Address_type pageAddress, bool hasPageAddress);
    void CopySelected(const oui::MultiLineSelPoint& p1, const oui::MultiLineSelPoint& p2) override;
    bool SelectAll() override;
    oui::LineIndex GetLineIndex(int offsetInPage) const override;
    void OnEnter() override;
    void OnPaintStart(std::shared_ptr<oui::CEditBox> editBox) override;
    void AsyncRememberCurrentPosition(oui::OperationPtr_type<orthia::GotoCompleteHandler_type> operation = nullptr);
    bool DoGotoOnPage(orthia::Address_type address);
    void MakeComment();

public:
    CDisasmWindow(std::function<oui::String()> getCaption,
        std::shared_ptr<orthia::CProgramModel> model);
    void PrepareParameters(UIState& state, int itemUid, DI_UINT64 initialAddressHint);

    // IUIStatefulWindow
    void ReloadState(const UIState& state) override;
    void SaveState(UIState& state) override;
    void SetActiveWorkspaceItem(int itemId) override;

    void DoGotoRequest(orthia::Address_type address);
};
