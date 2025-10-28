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
        orthia::Address_type startAddress,
        orthia::Address_type sizeInCommands,
        DisasmWriter& writer)
        :
        Parent_type(pTextPrinter,
            dianaMode,
            startAddress,
            sizeInCommands),
        m_writer(writer),
        m_firstVirtualOffset(startAddress),
        m_pTextPrinter(pTextPrinter)
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
        
        std::shared_ptr<DisasmLineContextTag> tag = std::make_shared<DisasmLineContextTag>();
        tag->address = address;
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
            if (tag->newOffset > tag->address)
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
                m_firstVirtualOffset = virtualOffset;
                m_firstPrint = false;
            }
            m_writer.lastCmdSize = result.iFullCmdSize;
        }
    }
    orthia::Address_type MemoryPrinter::GetRealFirstAddress() const
    {
        return m_firstVirtualOffset;
    }
}

