#include "ui_disasm_window.h"
#include "orthia_diana_print.h"
#include "orthia_pe.h"

// == Structure ==
// [PE HEADER]
// [SECTION HEADER]
// [FUNCTION HEADER]
// [INSTRUCTION HEADER]
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
void CDisasmWindow::SetActiveItem(int itemUid, DI_UINT64 initialAddressHint)
{
    SetActiveItemImpl(itemUid);
    m_peAddress = 0;
    m_peAddressEnd = 0;
    m_metaInfoPos = 0;

    auto item = m_model->GetItem(m_itemUid);
    if (item)
    {
        auto range = item->GetRangeInfo(initialAddressHint);
        m_peAddress = range.entryPoint;
    }
    ReloadVisibleData();
    Invalidate();
}
void CDisasmWindow::ReloadVisibleData()
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
    auto rangeInfo = item->GetRangeInfo(m_peAddress);

    const int maxStepForwardBytes = 1024;
    const int maxStepBackwardBytes = m_userSuppliedPeAddress ? 0: 256 ;

    orthia::Address_type routeStart = m_peAddress;
    if (auto moduleManager = item->GetModuleManager())
    {
        routeStart = moduleManager->QueryRouteStart(m_peAddress);
    }
    if (!routeStart || (m_peAddress - routeStart) > (orthia::Address_type)maxStepBackwardBytes)
    {
        // no route or it it too far away (which is strange, whatever)
        if (m_peAddress > maxStepBackwardBytes)
        {
            routeStart = m_peAddress - maxStepBackwardBytes;
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
    struct DisasmWriter:orthia::ITextPrinter
    {
        std::vector<oui::MultiLineViewItem> items;
        int lastCmdSize = 0;
        virtual void PrintLine(const std::wstring& line)
        {
            oui::MultiLineViewItem item;
            item.text = line;
            item.intTag = lastCmdSize;
            items.push_back(item);
        }
    }writer;

    class MemoryPrinter:public orthia::CSubrangeMemoryPrinter<diana::CMasmString>
    {
        using Parent_type = orthia::CSubrangeMemoryPrinter<diana::CMasmString>;

        DisasmWriter& m_writer;
        bool m_firstPrint = true;
        orthia::Address_type m_firstVirtualOffset;
        const char* m_pDataFlags = 0;
        orthia::Address_type m_routeStart = 0;
    public:
        MemoryPrinter(orthia::ITextPrinter* pTextPrinter,
            int dianaMode,
            orthia::Address_type startAddress,
            orthia::Address_type sizeInCommands,
            DisasmWriter& writer)
            : 
            Parent_type(pTextPrinter,
                dianaMode, 
                startAddress, 
                sizeInCommands),
            m_writer(writer),
            m_firstVirtualOffset(startAddress)
        {
            m_bytesIdent += 10;
            m_countOfSpacesAfterAddress = 3;
            m_printInvalidPages = true;
        }
        void SetFlags(const char* pDataFlags, orthia::Address_type routeStart)
        {
            m_pDataFlags = pDataFlags;
            m_routeStart = routeStart;
        }
        bool IsBadByte(orthia::Address_type virtualOffset) override
        {
            if (m_pDataFlags)
            {
                auto relativeOffset = virtualOffset - m_routeStart;
                auto & flag = m_pDataFlags[relativeOffset];
                return flag & orthia::WorkAddressData::dataFlags_Invalid;
            }
            return false;
        }
        void Preprocess(int iRes,
            ::DianaContext& context,
            ::DianaParserResult& result,
            orthia::Address_type virtualOffset,
            bool* pPrint,
            bool* pExit) override
        {
            Parent_type::Preprocess(iRes,
                context,
                result,
                virtualOffset,
                pPrint,
                pExit);
            if (*pPrint)
            {
                if (m_firstPrint)
                {
                    m_firstVirtualOffset = virtualOffset;
                    m_firstPrint = false;
                }
                m_writer.lastCmdSize = result.iFullCmdSize;
            }
        }
        orthia::Address_type GetRealFirstAddress() const
        {
            return m_firstVirtualOffset;
        }

    }printer(&writer, 
        rangeInfo.dianaMode,
        m_peAddress,
        requiredLinesCount,
        writer);

    // query data
    const orthia::Address_type maxSizeToUse = maxStepForwardBytes + (m_peAddress - routeStart);
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

    m_view->Init(std::move(writer.items));

    if (!m_userSuppliedPeAddress)
    {
        // check if can be adjusted
        m_peAddress = printer.GetRealFirstAddress();
    }

    // clear flag on any next move
    m_userSuppliedPeAddress = false;
}
void CDisasmWindow::CancelAllQueries()
{
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
    if (m_peAddress > count)
    {
        m_peAddress -= count;
    }
    else
    {
        m_peAddress = 0;
    }
    ReloadVisibleData();
    return true;
}
bool CDisasmWindow::ScrollDown(oui::MultiLineViewItem* item, int count) 
{
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
    int bytesCount = 0;
    int index = 0;
    for (auto it = m_view->VisibleItemsBegin(), it_end = m_view->VisibleItemsEnd(); it != it_end && index < countToUse; ++it, ++index)
    {
        bytesCount += it->intTag;
    }
    m_peAddress += bytesCount;
    m_userSuppliedPeAddress = true;
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
    {
        auto it = state.addresses.find(field_peAddress);
        if (it != state.addresses.end())
        {
            m_peAddress = it->second;
        }
    }
    m_peAddressEnd = 0;
    m_metaInfoPos = 0;
}

void CDisasmWindow::SaveState(UIState& state)
{
    state.addresses[field_peAddress] = m_peAddress;
}

void CDisasmWindow::SetActiveWorkspaceItem(int itemId)
{
    SetActiveItemImpl(itemId);
    ReloadVisibleData();
    Invalidate();

}