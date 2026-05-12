#include "ui_disasm_memory_writer.h"
#include "orthia_model_interfaces.h"

namespace oui
{
    // DisasmWriter
    void DisasmWriter::PrintLine(const orthia::PlatformString_type& line)
    {
        oui::MultiLineViewItem item;
        item.text = line;
        item.intTag = lastCmdSize;
        items.push_back(item);
    }
    void DisasmWriter::PrintLine(const orthia::PlatformString_type& line, const oui::TextMarkup& markup, std::shared_ptr<IMultilineViewTag> tag)
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
    void MemoryPrinter::SetEndAddress(const oui::LineIndex& endAddress)
    {
        m_endAddress = endAddress;
        m_haveEndAddress = true;
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
        const orthia::PlatformString_type& bytes,
        const orthia::PlatformString_type& command)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = oui::LineIndex(address, 0);
        PrintCommandEx(address, bytes, command, tag);
    }
    void MemoryPrinter::PrintCommand(const oui::LineIndex& address,
        const orthia::PlatformString_type& bytes,
        const orthia::PlatformString_type& command)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = address;
        PrintCommandEx(address.GetIndex(), bytes, command, tag);
    }
    void MemoryPrinter::PrintMetaInfo(const oui::LineIndex& address,
        const orthia::MarkupLine& line)
    {
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->index = address;

        m_currentBlock.clear();
        // pack address
        m_currentBlock.append(orthia::AddressToString(address.GetIndex(), m_dianaMode));
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size(), m_colors.address, g_region_id_address, oui::TextMarkup::flag_ManualHighlight);

        // pack spaces
        m_currentBlock.append(m_countOfSpacesAfterAddress, ORTHIA_TCSTR(' '));
        m_textMarkupBuilder.AddNextRange(m_countOfSpacesAfterAddress, m_colors.spaces);

        // pack "; "
        m_currentBlock.append(ORTHIA_TCSTR("; "));
        m_textMarkupBuilder.AddNextRange(2, m_colors.bytes);

        // pack text
        m_currentBlock.append(line.text.native);
        if (line.hasMarkup)
        {
            for (const auto& range : line.markup.ranges)
            {
                m_textMarkupBuilder.AddNextRange(range.sizeInTChars, range.colorProfile, range.id, range.flags);
            }
        }
        else
        {
            m_textMarkupBuilder.AddNextRange(line.text.native.size(), m_colors.generalMeta);
        }

        if (line.flags & orthia::MarkupLine::flags_HasXRefs)
        {
            if (m_referencesCache)
            {
                const orthia::CommonReferenceInfoArray_type* refs = nullptr;
                if (m_referencesCache->QueryReferences(address.GetIndex(), &refs) && refs)
                {
                    tag->xrefs = *refs;
                }
            }
        }
        m_pTextPrinter->PrintLine(m_currentBlock, m_textMarkupBuilder.Build(), tag);

        m_operands.clear();
    }

    Diana_LinkedAdditionalGroupInfo* MemoryPrinter::GetLinkedInfo()
    {
        auto pInfo = m_pDianaPrintContext->result.pInfo;
        if (!pInfo)
        {
            return 0;
        }
        return pInfo->m_pGroupInfo->m_pLinkedInfo;
    }
    void MemoryPrinter::PackCommand(const orthia::PlatformString_type& command, std::shared_ptr<DisasmLineContextTag> tag)
    {
        m_textMarkupBuilder.AddNextRange(0, m_colors.command, g_region_id_command);
        m_currentBlock.append(command);
    }
    void MemoryPrinter::PrintCommandEx(unsigned long long address,
        const orthia::PlatformString_type& bytes,
        const orthia::PlatformString_type& command,
        std::shared_ptr<DisasmLineContextTag> tag)
    {
        m_currentBlock.clear();
        // pack address
        m_currentBlock.append(orthia::AddressToString(address, m_dianaMode));
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size(), m_colors.address,   g_region_id_address, oui::TextMarkup::flag_ManualHighlight);

        // pack spaces
        m_currentBlock.append(m_countOfSpacesAfterAddress, ORTHIA_TCSTR(' '));
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
        m_currentBlock.resize(count, ORTHIA_TCSTR(' '));
        m_textMarkupBuilder.AddNextRange(m_currentBlock.size() - oldSize, m_colors.spaces);

        // pack command
        Diana_AnalyzeJumps(&m_pDianaPrintContext->result,
            address + m_pDianaPrintContext->result.iFullCmdSize,
            &tag->newOffset,
            &tag->absoluteAddress,
            &tag->linksToData);

        PackCommand(command, tag);
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

        // build comment section
        oldSize = m_currentBlock.size();
        auto addCommentSeparator = [&]()
        {
            if (m_currentBlock.size() == oldSize)
            {
                m_currentBlock.append(3, ORTHIA_TCSTR(' '));
                m_currentBlock.append(ORTHIA_TCSTR("; "));
            }
            else
            {
                m_currentBlock.append(1, ORTHIA_TCSTR(' '));
            }
        };

        auto linkedInfo = GetLinkedInfo();

        // read comment
        oui::String comment;
        if (auto storage = m_workspaceItem->GetPersistentStorage())
        {
            comment = storage->SyncReadComment(address);
        }
        
        if (!comment.native.empty())
        {
            addCommentSeparator();
            m_currentBlock.append(comment.native);
        }
        else
        if (tag->newOffset)
        {
            auto gotoAddress = tag->newOffset;
            OPERAND_SIZE  symAddress = 0;
            if (tag->linksToData)
            {
                // dereference
                auto data = m_workspaceItem->ReadData(tag->newOffset, m_workspaceItem->GetDianaMode());
                if (data.pDataStart && data.dataSize == m_workspaceItem->GetDianaMode())
                {
                    gotoAddress = Diana_ReadValue(data.pDataStart, m_workspaceItem->GetDianaMode());
                }
            }
            else
            {
                // TODO: move it to some utils, too low-level here
                int opSize = 2;
                switch (m_workspaceItem->GetDianaMode())
                {
                case 4:
                case 8:
                    opSize = 4;
                    break;
                }
                auto data = m_workspaceItem->ReadData(tag->newOffset, opSize+1);
                if (data.pDataStart && data.dataSize == opSize + 1 && (*(DI_CHAR*)data.pDataStart == 0xE9))
                {
                    symAddress = tag->newOffset + Diana_ReadValue(data.pDataStart + 1, opSize) + 5;
                }
            }

            auto nameInfo = m_workspaceItem->QueryAddressName(symAddress? symAddress:gotoAddress);
            auto name = orthia::GetPreferredName(nameInfo);
            if (!name.native.empty())
            {
                addCommentSeparator();
                m_currentBlock.append(name.native);
            }
        }

        // put arrows
        if (tag->newOffset && !tag->linksToData && 
            (linkedInfo && linkedInfo->flags & DIANA_GT_IS_JUMP))
        {
            addCommentSeparator();
            if (tag->newOffset > tag->index.GetIndex())
            {
#ifdef DIANA_HAS_POSIX
                m_currentBlock.append("\xe2\x86\x93"); // ↓ UTF-8
#else
                m_currentBlock.append(1, L'\x2193'); // ↓
#endif
            }
            else
            {
#ifdef DIANA_HAS_POSIX
                m_currentBlock.append("\xe2\x86\x91"); // ↑ UTF-8
#else
                m_currentBlock.append(1, L'\x2191'); // ↑
#endif
            }
        }

        // dump operands if no any analysis done
        if (m_currentBlock.size() == oldSize)
        {
            for (auto& op : m_operands)
            {
                auto nameInfo = m_workspaceItem->QueryAddressName(op.operand);
                if (!nameInfo.name.native.empty())
                {
                    addCommentSeparator();
                    m_currentBlock.append(nameInfo.name.native);
                }
            }
        }

        if (m_currentBlock.size() > oldSize)
        {
            m_textMarkupBuilder.AddNextRange(m_currentBlock.size() - oldSize, m_colors.bytes);
        }
        // done comments section

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
        DianaMemoryStream stream;
        DianaPrintContext ctx;
        m_pDianaPrintContext = &ctx;
        ::DianaParserResult& result = ctx.result;
        ::DianaContext& context = ctx.context;
        ctx.pStream = &stream.parent.parent;

        if (m_currentCommand >= m_sizeInCommands)
        {
            return;
        }
        bool reportNoData = false;
        if (!vmRange.HasData() || !pDataStart)
        {
            if (!m_printInvalidPages)
            {
                PrintCommand(vmRange.address, ORTHIA_TCSTR("??"), ORTHIA_TCSTR("???"));
                throw std::runtime_error("Memory access error");
            }
            reportNoData = true;
        }
        
        Diana_InitContext(&context, m_dianaMode);
        Diana_InitMemoryStream(&stream, (void*)pDataStart, (size_t)vmRange.size);

        oui::LineIndex virtualOffset = oui::LineIndex(vmRange.address, 0);
        OnStream(&ctx, virtualOffset, reportNoData);
    }
    void MemoryPrinter::OnStream(DianaPrintContext* pDianaPrintContext, oui::LineIndex virtualOffset, bool reportNoData)
    {
        m_pDianaPrintContext = pDianaPrintContext;
        ::DianaParserResult& result = m_pDianaPrintContext->result;
        auto & stream = *m_pDianaPrintContext->pStream;
        ::DianaContext& context = m_pDianaPrintContext->context;

        orthia::PlatformString_type temp, binaryData;
        size_t offsetInPage = 0;
        bool prevWasBad = false;
        orthia::MarkupRange markupRange;
        for (; m_currentCommand < m_sizeInCommands; )
        {
            if (m_haveEndAddress && m_endAddress < virtualOffset) 
            {
                break;
            }
            if (!(virtualOffset < m_startAddress))
            {
                auto commandsToDeliver = m_sizeInCommands - m_currentCommand;
                m_workspaceItem->QueryMarkupRange(virtualOffset.GetIndex(), virtualOffset.GetSubIndex(), (int)commandsToDeliver, markupRange, m_referencesCache);
                for (auto& line : markupRange.lines)
                {
                    PrintMetaInfo(virtualOffset, line);
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
                    if (!reportNoData) {
                        iRes = stream.parent.pReadFnc(&stream,
                            &data,
                            1,
                            &bytesRead);
                    }
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
                iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent, &result);
            }
            if (iRes == DI_END)
            {
                break;
            }
            bool print = true, exit = false;
            Preprocess(iRes, context, result, virtualOffset.GetIndex(), &print, &exit);
            if (iRes)
            {
                temp = orthia::ToHexString(result.bytes, result.bytesCount);
                if (print)
                {
                    if (prevWasBad)
                    {
                        PrintCommand(virtualOffset, ORTHIA_TCSTR("??"), ORTHIA_TCSTR("???"));
                    }
                    else
                    {
                        orthia::PlatformString_type dbCommand = ORTHIA_TCSTR("db");
                        dbCommand.append(m_spacesCount, ORTHIA_TCSTR(' '));
                        PrintCommand(virtualOffset, temp, dbCommand + temp);
                    }
                }
                ++offsetInPage;
                virtualOffset.IncIndex();
                DI_CHECK_CPP(stream.pMoveTo(&stream, offsetInPage));
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
                    PrintCommand(virtualOffset, ORTHIA_TCSTR("??"), ORTHIA_TCSTR("???"));
                }
                else
                {
                    temp = orthia::Utf8ToPlatformString(Parent_type::m_writer.Assign(&result, virtualOffset.GetIndex()));
                    binaryData = orthia::ToHexString(result.bytes, result.bytesCount);

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

