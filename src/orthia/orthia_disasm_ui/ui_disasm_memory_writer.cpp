#include "ui_disasm_memory_writer.h"
#include "orthia_model_interfaces.h"

namespace oui
{
    // DisasmWriter
    void DisasmWriter::PrintLine(const std::wstring& line)
    {
        oui::MultiLineViewItem item;
        item.text = line;
        item.intTag = lastCmdSize;
        items.push_back(item);
    }
    void DisasmWriter::PrintLine(const std::wstring& line, const oui::TextMarkup& markup, std::shared_ptr<IMultilineViewTag> tag)
    {
        oui::MultiLineViewItem item;
        item.text = line;
        item.intTag = lastCmdSize;
        item.markup = markup;
        item.interfaceTag = tag;
        items.push_back(item);
    }

    static void DianaStringOutputOnOpCallback(struct _DianaStringOutputContext* pContext,
        void* pUserContext,
        const char* pText, int size,
        OPERAND_SIZE operand, int opSize)
    {
        auto memoryPrinter = (MemoryPrinter*)pUserContext;
        if (opSize == memoryPrinter->GetDianaMode())
        {
            auto offset = pText - pContext->pBuffer;
            memoryPrinter->AddOperandPointer(operand, offset, size);
        }
    }

    // MemoryPrinter
    MemoryPrinter::MemoryPrinter(DisasmWriter* pTextPrinter,
        int dianaMode,
        const oui::LineIndex& startAddress,
        orthia::Address_type sizeInCommands,
        std::shared_ptr<orthia::IWorkPlaceItem> workspaceItem)
        :
        Parent_type(pTextPrinter,
            dianaMode,
            startAddress.GetIndex(),
            sizeInCommands),
        m_writer(*pTextPrinter),
        m_firstVirtualOffset(startAddress),
        m_pTextPrinter(pTextPrinter),
        m_workspaceItem(workspaceItem),
        m_startAddress(startAddress)
    {
        QueryDefaultColorProfile(m_colors);
        m_bytesIdent += 10;
        m_countOfSpacesAfterAddress = 3;
        m_printInvalidPages = true;
        SetSpacesCount(3);
        Parent_type::m_writer.SetOnOpCallback(DianaStringOutputOnOpCallback, this);
    }
    void MemoryPrinter::AddOperandPointer(OPERAND_SIZE operand, size_t offset, int size)
    {
        MemoryPrinterOperandInfo info;
        info.operand = operand;
        info.offset = offset;
        info.size = size;
        m_operands.push_back(info);
    }
    void MemoryPrinter::PrintCommand(unsigned long long address,
        const std::wstring& bytes,
        const std::wstring& command)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = oui::LineIndex(address, 0);
        PrintCommandEx(address, bytes, command, tag);
    }
    void MemoryPrinter::PrintCommand(const oui::LineIndex& address,
        const std::wstring& bytes,
        const std::wstring& command)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = address;
        PrintCommandEx(address.GetIndex(), bytes, command, tag);
    }
    void MemoryPrinter::PrintMetaInfo(const oui::LineIndex& address,
        const std::wstring& text)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = address;

        m_currentBlock.clear();
        // pack address
        if (m_dianaMode < 8)
        {
            m_currentBlock.append(orthia::ToWideStringAsHex((unsigned int)address.GetIndex()));
        }
        else
        {
            m_currentBlock.append(orthia::Address64ToString(address.GetIndex()));
        }
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size(), m_colors.address, g_region_id_address, oui::TextMarkup::flag_ManualHighlight);

        // pack spaces
        m_currentBlock.append(m_countOfSpacesAfterAddress, L' ');
        m_textMarkupBuilder.AddNextRange(m_countOfSpacesAfterAddress, m_colors.spaces);

        // pack spaces
        m_currentBlock.append(L"; ");
        m_textMarkupBuilder.AddNextRange(2, m_colors.bytes);

        // pack command
        auto oldSize = m_currentBlock.size();
        m_currentBlock.append(text);
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size() - oldSize, m_colors.generalMeta);

        m_pTextPrinter->PrintLine(m_currentBlock, m_textMarkupBuilder.Build(), tag);

        m_operands.clear();
    }
    void MemoryPrinter::PrintCommandEx(unsigned long long address,
        const std::wstring& bytes,
        const std::wstring& command,
        std::shared_ptr<DisasmLineContextTag> tag)
    {
        m_currentBlock.clear();
        // pack address
        if (m_dianaMode < 8)
        {
            m_currentBlock.append(orthia::ToWideStringAsHex((unsigned int)address));
        }
        else
        {
            m_currentBlock.append(orthia::Address64ToString(address));
        }
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size(), m_colors.address,   g_region_id_address, oui::TextMarkup::flag_ManualHighlight);

        // pack spaces
        m_currentBlock.append(m_countOfSpacesAfterAddress, L' ');
        m_textMarkupBuilder.AddNextRange(m_countOfSpacesAfterAddress, m_colors.spaces);

        // pack bytes
        m_currentBlock.append(bytes);
        m_textMarkupBuilder.AddNextRange(bytes.size(), m_colors.bytes);

        // pack spaces
        auto oldSize = m_currentBlock.size();
        int count = m_bytesIdent;
        if (count < (int)m_currentBlock.size())
        {
            count = (int)m_currentBlock.size() + 1;
        }
        m_currentBlock.resize(count, L' ');
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size() - oldSize, m_colors.spaces);

        // pack command
        m_currentBlock.append(command);
        size_t lastOffset = 0;
        for (auto& op : m_operands)
        {
            m_textMarkupBuilder.AddNextRange(op.offset - lastOffset, m_colors.command);
            m_textMarkupBuilder.AddNextRange(op.size, m_colors.operand, g_region_id_operand);
            lastOffset = op.offset + op.size;
        }
        if (command.size() > lastOffset)
        {
            m_textMarkupBuilder.AddNextRange(command.size() - lastOffset, m_colors.command);
        }
        

        Diana_AnalyzeJumps(&m_pDianaPrintContext->result, 
            address + m_pDianaPrintContext->result.iFullCmdSize, 
            &tag->newOffset, 
            &tag->absoluteAddress, 
            &tag->linksToData);

        if (tag->newOffset && !tag->linksToData && 
            (m_pDianaPrintContext->result.pInfo->m_pGroupInfo->m_pLinkedInfo->flags & DIANA_GT_IS_JUMP))
        {
            oldSize = m_currentBlock.size();

            m_currentBlock.append(3, L' ');
            m_currentBlock.append(L"; ");
            if (tag->newOffset > tag->index.GetIndex())
            {
                m_currentBlock.append(1, L'\x2193');
            }
            else
            {
                m_currentBlock.append(1, L'\x2191');
            }
            m_textMarkupBuilder.AddNextRange(bytes.size(), m_colors.bytes);
            m_textMarkupBuilder.AddNextRange(m_currentBlock.size() - oldSize, m_colors.bytes);
        }

        m_pTextPrinter->PrintLine(m_currentBlock, m_textMarkupBuilder.Build(), tag);

        m_operands.clear();
    }

    void MemoryPrinter::SetFlags(const char* pDataFlags, orthia::Address_type routeStart)
    {
        m_pDataFlags = pDataFlags;
        m_routeStart = routeStart;
    }

    void MemoryPrinter::OnRange(const orthia::VmMemoryRangeInfo& vmRange,
        const char* pDataStart)
    {
        if (m_currentCommand >= m_sizeInCommands)
        {
            return;
        }
        bool reportNoData = false;
        if (!vmRange.HasData())
        {
            if (!m_printInvalidPages)
            {
                PrintCommand(vmRange.address, L"??", L"???");
                throw std::runtime_error("Memory access error");
            }
            reportNoData = true;
        }

        DianaPrintContext ctx;
        m_pDianaPrintContext = &ctx;
        ::DianaParserResult& result = ctx.result;
        ::DianaMemoryStream& stream = ctx.stream;
        ::DianaContext& context = ctx.context;

        Diana_InitContext(&context, m_dianaMode);
        Diana_InitMemoryStream(&stream, (void*)pDataStart, (size_t)vmRange.size);
        
        std::wstring temp, binaryData;
        oui::LineIndex virtualOffset = oui::LineIndex(vmRange.address, 0);
        size_t offsetInPage = 0;
        bool prevWasBad = false;
        orthia::MarkupRange markupRange;
        for (; m_currentCommand < m_sizeInCommands; )
        {
            if (!(virtualOffset < m_startAddress))
            {
                auto commandsToDeliver = m_sizeInCommands - m_currentCommand;
                m_workspaceItem->QueryMarkupRange(virtualOffset.GetIndex(), virtualOffset.GetSubIndex(), (int)commandsToDeliver, markupRange);
                for (auto& line : markupRange.lines)
                {
                    PrintMetaInfo(virtualOffset, line.native);
                    virtualOffset.IncSubIndex();
                    ++m_currentCommand;
                }
                if (m_currentCommand >= m_sizeInCommands)
                {
                    break;
                }
            }
            int iRes = 0;
            if (reportNoData || this->IsBadByte(virtualOffset.GetIndex()))
            {
                prevWasBad = true;
                int bytesRead = 0;
                char data = 0;

                if (context.cacheSize)
                {
                    Diana_CacheEatOneSafe(&context);
                    iRes = 0;
                }
                else
                {
                    iRes = stream.parent.parent.parent.pReadFnc(&stream,
                        &data,
                        1,
                        &bytesRead);
                }
                result.iFullCmdSize = 1;
                result.iLinkedOpCount = 0;
                result.pInfo = Diana_GetNopInfo();
            }
            else
            {
                if (prevWasBad)
                {
                    Diana_ClearCache(&context);
                }
                prevWasBad = false;
                iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent, &result);
            }
            if (iRes == DI_END)
            {
                break;
            }
            bool print = true, exit = false;
            Preprocess(iRes, context, result, virtualOffset.GetIndex(), &print, &exit);
            if (iRes)
            {
                temp = orthia::ToHexString(pDataStart + offsetInPage, 1);
                if (print)
                {
                    if (prevWasBad)
                    {
                        PrintCommand(virtualOffset, L"??", L"???");
                    }
                    else
                    {
                        std::wstring dbCommand = L"db";
                        dbCommand.append(m_spacesCount, L' ');
                        PrintCommand(virtualOffset, temp, dbCommand + temp);
                    }
                }
                ++offsetInPage;
                virtualOffset.IncIndex();
                DI_CHECK_CPP(stream.parent.parent.pMoveTo(&stream.parent.parent, offsetInPage));
                Diana_ClearCache(&context);

                if (exit)
                {
                    break;
                }
                continue;
            }
            if (print)
            {
                ++m_currentCommand;
                if (prevWasBad)
                {
                    PrintCommand(virtualOffset, L"??", L"???");
                }
                else
                {
                    temp = orthia::ToWideString(Parent_type::m_writer.Assign(&result, virtualOffset.GetIndex()));
                    binaryData = orthia::ToHexString(pDataStart + offsetInPage, result.iFullCmdSize);

                    PrintCommand(virtualOffset, binaryData, temp);
                }
            }
            offsetInPage += result.iFullCmdSize;
            virtualOffset.IncIndex(result.iFullCmdSize);

            if (exit)
            {
                break;
            }
        }

        m_pDianaPrintContext = 0;
    }

    bool MemoryPrinter::IsBadByte(orthia::Address_type virtualOffset)
    {
        if (m_pDataFlags)
        {
            auto relativeOffset = virtualOffset - m_routeStart;
            auto& flag = m_pDataFlags[relativeOffset];
            return flag & orthia::WorkAddressData::dataFlags_Invalid;
        }
        return false;
    }
    void MemoryPrinter::Preprocess(int iRes,
        ::DianaContext& context,
        ::DianaParserResult& result,
        orthia::Address_type virtualOffset,
        bool* pPrint,
        bool* pExit)
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
                m_firstVirtualOffset = oui::LineIndex(virtualOffset, 0);
                m_firstPrint = false;
            }
            m_writer.lastCmdSize = result.iFullCmdSize;
        }
    }
    oui::LineIndex  MemoryPrinter::GetRealFirstAddress() const
    {
        return m_firstVirtualOffset;
    }
}

