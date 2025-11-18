#include "ui_disasm_window.h"
#include "ui_disasm_memory_writer.h"
#include "orthia_pe.h"
#include "oui_goto_dialog.h"
#include "oui_disasm_colors.h"

// == Structure ==
// [PE HEADER]
// [SECTION HEADER]
// [FUNCTION HEADER]
// [INSTRUCTION HEADER]

std::shared_ptr<oui::DisasmLineContextTag> GetDisasmTag(std::shared_ptr<oui::IMultilineViewTag> interfaceTag)
{
    return std::static_pointer_cast<oui::DisasmLineContextTag>(interfaceTag);
}

std::shared_ptr<oui::DisasmLineContextTag> GetDisasmTag(oui::MultiLineViewItem& item)
{
    return std::static_pointer_cast<oui::DisasmLineContextTag>(item.interfaceTag);
}


CDisasmWindow::CDisasmWindow(std::function<oui::String()> getCaption,
    std::shared_ptr<orthia::CProgramModel> model)
    :
        m_model(model),
        oui::SimpleBrush<oui::CPanelWindow>(getCaption)
{
    m_colorProfile = std::make_shared<oui::DialogColorProfile>();
    QueryDefaultColorProfile(*m_colorProfile);

    oui::IMultiLineViewOwner* param = this;
    m_view = std::make_shared<oui::CMultiLineView>(m_colorProfile, param, false);
}

void CDisasmWindow::SetActiveItemImpl(int itemUid)
{
    m_itemUid = itemUid;
}
void CDisasmWindow::PrepareParameters(UIState& state, int itemUid, DI_UINT64 initialAddressHint)
{
    DI_UINT64 address = 0;
    auto item = m_model->GetItem(itemUid);
    if (item)
    {
        auto range = item->GetRangeInfo(initialAddressHint);
        address = range.entryPoint;
    }
   
    state.addresses[field_peAddress_index] = address;
    state.addresses[field_peAddress_subIndex] = 0;
}

void CDisasmWindow::ReloadVisibleData(const ReloadVisibleDataContext& context)
{
    const int requiredLinesCount = m_view->GetSize().height;
    if (!requiredLinesCount)
    {
        return;
    }
    auto item = m_model->GetItem(m_itemUid);
    if (!item)
    {
        return;
    }

    auto rangeInfo = item->GetRangeInfo(m_peAddress.GetIndex());

    const int maxStepForwardBytes = 1024;
    const int maxStepBackwardBytes = context.scrollUp ? 256: 0;

    orthia::Address_type routeStart = 0;
    if (auto moduleManager = item->GetModuleManager())
    {
        routeStart = moduleManager->QueryRouteStart(m_peAddress.GetIndex());
    }
    if (!routeStart || (m_peAddress.GetIndex() - routeStart) > (orthia::Address_type)maxStepBackwardBytes)
    {
        // no route or it it too far away (which is strange, whatever)
        if (m_peAddress.GetIndex() > maxStepBackwardBytes)
        {
            routeStart = m_peAddress.GetIndex() - maxStepBackwardBytes;
        }
        else
        {
            routeStart = 0;
        }
    }

    {
        // check if route start is in another range
        auto routeInfo = item->GetRangeInfo(routeStart);
        if (routeInfo.address != rangeInfo.address)
        {
            routeStart = rangeInfo.address;
        }
    }
    oui::DisasmWriter writer;
    oui::MemoryPrinter printer(&writer,
        rangeInfo.dianaMode,
        m_peAddress,
        requiredLinesCount,
        item);

    // determine final size
    const orthia::Address_type maxSizeToUse = maxStepForwardBytes + (m_peAddress.GetIndex() - routeStart);

    // query metadata
    std::vector<orthia::CommonRangeInfo> rangeInfoVec;
    if (auto moduleManager = item->GetModuleManager())
    {
        moduleManager->QueryReferencesToInstructionsRange(routeStart, routeStart + maxSizeToUse, &rangeInfoVec);
    }
    // query data
    auto data = item->ReadData(routeStart, maxSizeToUse);
    if (!data.dataSize)
    {
        m_view->Clear();
        return;
    }

    orthia::VmMemoryRangeInfo vmRangeInfo;
    vmRangeInfo.address = routeStart;
    vmRangeInfo.size = maxSizeToUse;
    vmRangeInfo.flags = vmRangeInfo.flags_hasData;

    if (data.rangeFlags & orthia::WorkAddressData::flags_FullInvalid)
    {
        vmRangeInfo.flags = 0;
    }
    printer.SetFlags(data.pDataFlags, routeStart);
    printer.OnRange(vmRangeInfo, data.pDataStart);

    m_view->Init(std::move(writer.items), false);

    if (context.scrollUp)
    {
        // check if can be adjusted
        m_peAddress = printer.GetRealFirstAddress();
        m_view->FixupTopSelectionRange();
    }
}
void CDisasmWindow::CancelAllQueries()
{
}
void CDisasmWindow::CopySelected(const oui::MultiLineSelPoint& p1, const oui::MultiLineSelPoint& p2)
{
}
void CDisasmWindow::OnEnter()
{
    auto item = m_model->GetItem(m_itemUid);
    if (!item)
    {
        return;
    }

    auto lineItem = m_view->GetCurrentItem();
    if (lineItem.interfaceTag)
    {
        auto tag = GetDisasmTag(lineItem);
        if (tag && tag->newOffset)
        {
            auto gotoAddress = tag->newOffset;
            if (tag->linksToData)
            {
                // dereference
                auto data = item->ReadData(tag->newOffset, item->GetDianaMode());
                if (data.pDataStart && data.dataSize == item->GetDianaMode())
                {
                    gotoAddress = Diana_ReadValue(data.pDataStart, item->GetDianaMode());
                }
            }


            if (auto storage = item->GetPersistentStorage())
            {
                auto operation = std::make_shared<oui::Operation<orthia::GotoCompleteHandler_type>>(
                    this->GetThread(),
                    [](orthia::Address_type address, int error) {
                    return oui::fsui::OpenResult();
                });

                AsyncRememberCurrentPosition(operation);

                storage->AsyncUpdateGotoInfo(this->GetThread(),
                    operation,
                    gotoAddress,
                    0,
                    0);
            }
            DoGoto(gotoAddress, 0, false);
        }
    }
}
bool CDisasmWindow::SelectAll()
{
    return false;
}
void CDisasmWindow::OnPaintStart(std::shared_ptr<oui::CEditBox> editBox)
{
    auto& ranges = m_view->GetPrevSelectedRanges();
    if (ranges.empty())
    {
        return;
    }
    oui::LineIndex index;
    if (m_view->PaintInProgress())
    {
        index = m_view->GetCurrentPaintedLineIndex();
    }
    else
    {
        index = m_view->GetCurrentLineIndex();
    }
    for (auto& r : ranges)
    {
        auto pair = m_view->GetItem(r.offsetInPage);
        auto tag = GetDisasmTag(pair.first);

        if (tag && tag->newOffset == index.GetIndex())
        {
            // highlight it
            editBox->HighlightRegion(oui::g_region_id_address);
        }
    }
}

oui::LineIndex CDisasmWindow::GetLineIndex(int offsetInPage) const
{
    oui::LineIndex index;
    auto pair = m_view->GetItem(offsetInPage);
    auto tag = GetDisasmTag(pair.first);
    if (tag)
    {
        if (pair.second)
        {
            index = tag->index;
        }
        else
        {
            // return address of last command + size of command
            index = oui::LineIndex(tag->index.GetIndex(), tag->index.GetSubIndex() + 1);
        }
    }
    return index;
}
bool CDisasmWindow::ScrollUp(oui::MultiLineViewItem* item, int count) 
{
    if (count > 1)
    {
        // assume it's a page request
        const int maxLinesCount = m_view->GetSize().height;
        if (m_view->GetCursorYPos() > maxLinesCount / 3)
        {
            // just move cursor this time
            m_view->SetCursorYPos(0);
            return true;
        }
        --count;
    }
    if (count <= m_peAddress.GetSubIndex())
    {
        m_peAddress = oui::LineIndex(m_peAddress.GetIndex(), m_peAddress.GetSubIndex() - count);
    }
    else
    {       
        if (m_peAddress.GetIndex() > count)
        {
            m_peAddress = oui::LineIndex(m_peAddress.GetIndex() - count, 0);
        }
        else
        {
            m_peAddress = oui::LineIndex(0, 0);
        }
    }
    ReloadVisibleDataContext context;
    context.scrollUp = true;
    ReloadVisibleData(context);
    return true;
}
bool CDisasmWindow::ScrollDown(oui::MultiLineViewItem* item, int count) 
{
    auto workplaceItem = m_model->GetItem(m_itemUid);
    if (!workplaceItem)
    {
        return true;
    }

    const int maxLinesCount = m_view->GetSize().height;
    int countToUse = count;
    if (count > 1)
    {
        // assume it's a page request
        if (m_view->GetCursorYPos() < (2 * maxLinesCount) / 3)
        {
            // just move cursor this time
            m_view->SetCursorYPos(maxLinesCount);
            return true;
        }
        // don't scroll the entire screen
        // 1) it looks better with up logic which is based on random guess
        // 2) it gives some context to user
        countToUse /= 2;
    }
    // count instructions here
    std::shared_ptr<oui::IMultilineViewTag> interfaceTag;
    int index = -1;
    for (auto it = m_view->VisibleItemsBegin(), it_end = m_view->VisibleItemsEnd(); it != it_end && index < countToUse; ++it, ++index)
    {
        interfaceTag = it->interfaceTag;
    }
    auto disasm = GetDisasmTag(interfaceTag);
    if (disasm)
    {
        m_peAddress = disasm->index;
    }
    ReloadVisibleData();
    return true;
}
void CDisasmWindow::ConstructChilds()
{
    AddChild(m_view);
}
void CDisasmWindow::OnResize()
{
    int prevHeight = m_view->GetSize().height;
    const oui::Rect clientRect = GetClientRect();
    
    m_view->Resize(clientRect.size);

    ReloadVisibleData();
}
void CDisasmWindow::SetFocusImpl()
{
    m_view->SetFocus();
}

void CDisasmWindow::ReloadState(const UIState& state)
{
    std::uint64_t index = 0;
    int subIndex = 0;
    Apply(state.addresses, field_peAddress_index,
        [&](auto& value) { index = value; },
        [&]() { index = 0; }
    );
    Apply(state.addresses, field_peAddress_subIndex,
        [&](auto& value) { subIndex = (int)value; },
        [&]() { subIndex = 0; }
    );
    m_peAddress = oui::LineIndex(index, subIndex);
    m_metaInfoPos = 0;
}

void CDisasmWindow::SaveState(UIState& state)
{
    state.addresses[field_peAddress_index] = m_peAddress.GetIndex();
    state.addresses[field_peAddress_subIndex] = m_peAddress.GetSubIndex();
}

void CDisasmWindow::SetActiveWorkspaceItem(int itemId)
{
    SetActiveItemImpl(itemId);
    ReloadVisibleData();
    Invalidate();
}
bool CDisasmWindow::DoGotoOnPage(orthia::Address_type address)
{
    oui::LineIndex lineIndex(address, 0);
    return m_view->SetCursorYPos(lineIndex);
}
void CDisasmWindow::DoGoto(orthia::Address_type address, orthia::Address_type pageAddress, bool hasPageAddress)
{
    if (!hasPageAddress)
    {
        if (DoGotoOnPage(address))
        {
            Invalidate();
            return;
        }
    }
    std::uint64_t targetAddress = 0;
    if (hasPageAddress)
    {
        targetAddress = pageAddress;
        auto wndHeight = m_view->GetClientRect().size.height;
        if (wndHeight)
        {
            if (address > wndHeight && targetAddress < (address - wndHeight))
            {
                targetAddress = address - wndHeight + 1;
            }
        }
    }
    else
    {
        targetAddress = address;
    }
    m_peAddress = oui::LineIndex(targetAddress, 0);
    m_view->Clear();
    m_view->SetCursorYPos(0);
    ReloadVisibleData();
    if (hasPageAddress && !DoGotoOnPage(address))
    {
        m_peAddress = oui::LineIndex(address, 0);
        m_view->Clear();
        ReloadVisibleData();
    }
    Invalidate();
}
void CDisasmWindow::AsyncRememberCurrentPosition(oui::OperationPtr_type<orthia::GotoCompleteHandler_type> operation)
{
    auto activeItem = m_model->GetActiveItem();
    if (!activeItem) 
    {
        return;
    }
    if (!activeItem->GetPersistentStorage()) 
    {
        return;
    }
    if (!operation)
    {
        operation = std::make_shared<oui::Operation<orthia::GotoCompleteHandler_type>>(
            this->GetThread(),
            [this](orthia::Address_type address, int error) {
            return oui::fsui::OpenResult();
        });
    }
    auto currentItem = m_view->GetCurrentLineIndex();

    activeItem->GetPersistentStorage()->AsyncUpdateGotoInfo(this->GetThread(),
        operation,
        currentItem.GetIndex(),
        orthia::IPeristentItemStorage::goto_flags_history_mode,
        m_peAddress.GetIndex());
}

void CDisasmWindow::DoGotoRequest(orthia::Address_type address)
{
    AsyncRememberCurrentPosition();
    DoGoto(address, 0, false);
}

void CDisasmWindow::Event_Goto(int scanFlags)
{
    // create open dialog
    oui::CommonDialogStrings dialogStrings;
    GetCommonDialogStrings(ORTHIA_TCSTR("ui.dialog.goto"), dialogStrings);

    auto activeItem = m_model->GetActiveItem();
    if (!activeItem) 
    {
        return;
    }
    if (!activeItem->GetPersistentStorage())
    {
        return;
    }
    auto dialog = AddChildAndInit_t(std::make_shared<oui::CGotoDialog>(dialogStrings,
        [=](orthia::Address_type address, int error) {

        if (!error)
        {
            DoGotoRequest(address);
        }
        return oui::fsui::OpenResult();
    },
        activeItem->GetPersistentStorage(),
        activeItem,
        scanFlags));
    dialog->Dock();
}

bool CDisasmWindow::ProcessEvent(oui::InputEvent& evt, oui::WindowEventContext& evtContext)
{
    oui::CConsole* console = GetConsole();
    if (!console)
    {
        return false;
    }

    if (evt.keyEvent.valid)
    {
        bool handled = false;
        switch (evt.keyEvent.virtualKey)
        {
        case oui::VirtualKey::Backspace:
            {
                auto activeItem = m_model->GetActiveItem();
                if (activeItem)
                {
                    if (auto storage = activeItem->GetPersistentStorage())
                    {
                        auto operation = std::make_shared<oui::Operation<orthia::FetchCompleteHandler_type>>(
                            this->GetThread(),
                            [this](orthia::Address_type address, int error, orthia::Address_type pageAddress) {
                                if (!error)
                                {
                                    DoGoto(address, pageAddress, true);
                                }
                                return oui::fsui::OpenResult();
                        });
                        storage->AsyncFetchPrevHistory(this->GetThread(), operation);
                    }
                }
                break;
            }
        case oui::VirtualKey::kG:
            if (!evt.keyState.HasModifiers() || evt.keyState.HasJustCtrl())
            {
                Event_Goto();
                handled = true;
            }
            break;

        case oui::VirtualKey::kH:
            if (!evt.keyState.HasModifiers() || evt.keyState.HasJustCtrl())
            {
                Event_Goto(orthia::IPeristentItemStorage::goto_flags_history_mode);
                handled = true;
            }
            break;

        default:
            break;
        }
        if (handled)
        {
            Invalidate();
            return true;
        }
    }
    return Parent_type::ProcessEvent(evt, evtContext);
}
