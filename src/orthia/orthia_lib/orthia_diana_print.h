#pragma once

#include "orthia_common_print.h"

namespace orthia
{

template<class AsmCommandWriterType>
class CVmAsmMemoryPrinter:public IVmMemoryRangesTarget
{
    ITextPrinter * m_pTextPrinter;
    int m_dianaMode;
    AsmCommandWriterType m_writer;
    std::wstring m_currentBlock;
    Address_type m_sizeInCommands;
    Address_type m_currentCommand;
public:
    CVmAsmMemoryPrinter(ITextPrinter * pTextPrinter,
                        int dianaMode,
                        Address_type sizeInCommands)
        :
            m_pTextPrinter(pTextPrinter),
            m_dianaMode(dianaMode),
            m_sizeInCommands(sizeInCommands),
            m_currentCommand(0)
    {
    }
    void PrintCommand(unsigned long long address,
        const std::wstring & bytes,
        const std::wstring & command)
    {
        m_currentBlock.clear();
        if (m_dianaMode < 8)
        {
            m_currentBlock.append(orthia::ToWideStringAsHex((unsigned int)address));
        }
        else
        {
            m_currentBlock.append(Address64ToString(address));
        }
        m_currentBlock.append(L" ");
        m_currentBlock.append(bytes);

        int count = 25;
        if (m_dianaMode == 8)
        {
            count = 31;
        }
        if (count < (int)m_currentBlock.size())
        {
            count = (int)m_currentBlock.size()+1;
        }
        m_currentBlock.resize(count, L' ');
        m_currentBlock.append(command);
        m_pTextPrinter->PrintLine(m_currentBlock);
    }
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        if (m_currentCommand >= m_sizeInCommands)
        {
            return;
        }
        if (!vmRange.HasData())
        {
            PrintCommand(vmRange.address, L"??", L"???");
            throw std::runtime_error("Memory access error");
        }
    
        ::DianaParserResult result;
        ::DianaMemoryStream stream;
        ::DianaContext context;

        Diana_InitContext(&context, m_dianaMode);
        Diana_InitMemoryStream(&stream, (void *)pDataStart, (size_t)vmRange.size);

        std::wstring temp, binaryData;
        Address_type virtualOffset = vmRange.address;
        size_t offsetInPage = 0;
        for(;m_currentCommand < m_sizeInCommands; ++m_currentCommand)
        {
            int iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent,  &result);
            if (iRes == DI_END)
            {
                break;
            }
            if (iRes)
            {
                temp = orthia::ToHexString(pDataStart + offsetInPage, 1);
                PrintCommand(virtualOffset, temp, L"db " + temp);
                
                ++offsetInPage;
                ++virtualOffset;
                DI_CHECK_CPP(stream.parent.parent.pMoveTo(&stream.parent.parent, offsetInPage));
                Diana_ClearCache(&context);
                continue;
            }
            temp = orthia::ToWideString(m_writer.Assign(&result, virtualOffset));
            binaryData = orthia::ToHexString(pDataStart + offsetInPage, result.iFullCmdSize);
            PrintCommand(virtualOffset, binaryData, temp);
            offsetInPage += result.iFullCmdSize;
            virtualOffset += result.iFullCmdSize;
        }
    }
};


}


