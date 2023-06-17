#pragma once

#include "orthia_common_print.h"

namespace orthia
{

    template<class AsmCommandWriterType>
    class CVmAsmMemoryPrinter:public orthia::IVmMemoryRangesTarget
    {
    protected:
        orthia::ITextPrinter* m_pTextPrinter;
        int m_dianaMode;
        AsmCommandWriterType m_writer;
        std::wstring m_currentBlock;
        orthia::Address_type m_sizeInCommands;
        orthia::Address_type m_currentCommand;
    public:
        CVmAsmMemoryPrinter(orthia::ITextPrinter* pTextPrinter,
            int dianaMode,
            orthia::Address_type sizeInCommands)
            :
            m_pTextPrinter(pTextPrinter),
            m_dianaMode(dianaMode),
            m_sizeInCommands(sizeInCommands),
            m_currentCommand(0)
        {
        }
        void PrintCommand(unsigned long long address,
            const std::wstring& bytes,
            const std::wstring& command)
        {
            m_currentBlock.clear();
            if (m_dianaMode < 8)
            {
                m_currentBlock.append(orthia::ToWideStringAsHex((unsigned int)address));
            }
            else
            {
                m_currentBlock.append(orthia::Address64ToString(address));
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
                count = (int)m_currentBlock.size() + 1;
            }
            m_currentBlock.resize(count, L' ');
            m_currentBlock.append(command);
            m_pTextPrinter->PrintLine(m_currentBlock);
        }
        virtual void Preprocess(int iRes, 
            ::DianaContext& context, 
            ::DianaParserResult& result, 
            orthia::Address_type virtualOffset, 
            bool* pPrint, 
            bool* pExit)
        {
            *pPrint = true;
            *pExit = false;
        }
        virtual void OnRange(const orthia::VmMemoryRangeInfo& vmRange,
            const char* pDataStart)
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
            Diana_InitMemoryStream(&stream, (void*)pDataStart, (size_t)vmRange.size);

            std::wstring temp, binaryData;
            orthia::Address_type virtualOffset = vmRange.address;
            size_t offsetInPage = 0;
            for (; m_currentCommand < m_sizeInCommands; ++m_currentCommand)
            {
                int iRes = Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent, &result);
                if (iRes == DI_END)
                {
                    break;
                }
                bool print = true, exit = false;
                Preprocess(iRes, context, result, virtualOffset, &print, &exit);
                if (iRes)
                {
                    temp = orthia::ToHexString(pDataStart + offsetInPage, 1);
                    if (print)
                    {
                        PrintCommand(virtualOffset, temp, L"db " + temp);
                    }
                    ++offsetInPage;
                    ++virtualOffset;
                    DI_CHECK_CPP(stream.parent.parent.pMoveTo(&stream.parent.parent, offsetInPage));
                    Diana_ClearCache(&context);

                    if (exit)
                    {
                        break;
                    }
                    continue;
                }
                temp = orthia::ToWideString(m_writer.Assign(&result, virtualOffset));
                binaryData = orthia::ToHexString(pDataStart + offsetInPage, result.iFullCmdSize);
                if (print)
                {
                    PrintCommand(virtualOffset, binaryData, temp);
                }
                offsetInPage += result.iFullCmdSize;
                virtualOffset += result.iFullCmdSize;

                if (exit)
                {
                    break;
                }
            }
        }
    };

    template<class AsmCommandWriterType>
    class CSubrangeMemoryPrinter:public CVmAsmMemoryPrinter<AsmCommandWriterType>
    {
        orthia::Address_type m_startAddress;
        orthia::Address_type m_sizeInCommands;
        orthia::Address_type m_reportedCommands;

    public:
        CSubrangeMemoryPrinter(orthia::ITextPrinter* pTextPrinter,
            int dianaMode,
            orthia::Address_type startAddress,
            orthia::Address_type sizeInCommands)
           
            :
                CVmAsmMemoryPrinter<AsmCommandWriterType>(pTextPrinter, dianaMode, INT_MAX),
                m_startAddress(startAddress),
                m_sizeInCommands(sizeInCommands),
                m_reportedCommands(0)
        {
        }
        void Preprocess(int iRes,
            ::DianaContext& context,
            ::DianaParserResult& result,
            orthia::Address_type virtualOffset,
            bool* pPrint,
            bool* pExit)
        {
            *pExit = false;

            orthia::Address_type virtualEndOfCommand = virtualOffset + result.iFullCmdSize;
            if (virtualEndOfCommand < m_startAddress)
            {
                // just skip the backward step
                *pPrint = false;
                return;
            }
            *pPrint = true;
            ++m_reportedCommands;

            if (m_reportedCommands >= m_sizeInCommands)
            {
                *pExit = true;
            }
        }
    };
}


