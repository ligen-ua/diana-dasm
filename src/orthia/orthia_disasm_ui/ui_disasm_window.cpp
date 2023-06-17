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
void CDisasmWindow::SetActiveItem(int itemUid)
{
    m_itemUid = itemUid;
    m_peAddress = 0;
    m_peAddressEnd = 0;
    m_metaInfoPos = 0;

    auto item = m_model->GetItem(m_itemUid);
    if (item)
    {
        const auto& context = item->GetFile()->GetImpl();
        auto imageBase = item->GetFile()->GetImageBase();
        m_peAddress = imageBase + context->mappedPE.pImpl->addressOfEntryPoint;
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
    auto context = item->GetFile()->GetImpl();

    const int maxStepForwardBytes = 1024;
    const int maxStepBackwardBytes = m_userSuppliedPeAddress ? 0: 256 ;
    
    const auto& mappedPE = item->GetFile()->GetMappedPeFile();
    auto imageBase = item->GetFile()->GetImageBase();
    if (m_peAddress < imageBase)
    {
        m_peAddress = imageBase;
    }
    else
    if (m_peAddress >= imageBase + (DI_UINT64)mappedPE.size())
    {
        m_view->Clear();
        return;
    }
    
    auto routeStart = item->GetModuleManager()->QueryRouteStart(m_peAddress);
    if (!routeStart || (m_peAddress - routeStart) > (orthia::Address_type)maxStepBackwardBytes)
    {
        // no route or it it too far away (which is strange, whatever)
        routeStart = m_peAddress - maxStepBackwardBytes;
        if (routeStart < imageBase)
        {
            routeStart = imageBase;
        }
    }

    // final check bounds
    auto routeOffset = routeStart - imageBase;
    if (routeOffset > mappedPE.size())
    {
        m_view->Clear();
        return;
    }
    if (m_peAddress < routeStart)
    {
        // something is terribly wrong
        m_view->Clear();
        return;
    }
    const orthia::Address_type preferrableSizeInBytes = maxStepForwardBytes + (m_peAddress - routeStart);
    orthia::Address_type end = preferrableSizeInBytes;
    orthia::Address_type maxSizeToUse = 0;
    if (Diana_SafeAdd(&end, routeOffset) == DI_SUCCESS && end <= (orthia::Address_type)mappedPE.size())
    {
        // end is valid, use it
        maxSizeToUse = end - routeOffset;
    }
    else
    {
        // any overflow, default to module's end
        maxSizeToUse = mappedPE.size() - routeOffset;
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
        context->mappedPE.pImpl->dianaMode, 
        m_peAddress,
        requiredLinesCount,
        writer);

    orthia::VmMemoryRangeInfo rangeInfo;
    rangeInfo.address = routeStart;
    rangeInfo.size = maxSizeToUse;
    rangeInfo.flags = rangeInfo.flags_hasData;
    printer.OnRange(rangeInfo, mappedPE.data() + routeOffset);

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



