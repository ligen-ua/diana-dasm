#pragma once

#include "orthia_common_print.h"

namespace orthia
{

    template<class AsmCommandWriterType>
    class CVmAsmMemoryPrinter:public orthia::IVmMemoryRangesTarget
    {
    public:
        struct DianaPrintContext
        {
            ::DianaParserResult result;
            ::DianaMovableReadStream* pStream = 0;
            ::DianaContext context;
        };
    protected:

        orthia::ITextPrinter* m_pTextPrinter;
        int m_dianaMode;
        AsmCommandWriterType m_writer;
        orthia::PlatformString_type m_currentBlock;
        orthia::Address_type m_sizeInCommands;
        orthia::Address_type m_currentCommand;
        int m_bytesIdent;
        int m_countOfSpacesAfterAddress;
        bool m_printInvalidPages;
        int m_spacesCount;
        DianaPrintContext* m_pDianaPrintContext;
    public:
        CVmAsmMemoryPrinter(orthia::ITextPrinter* pTextPrinter,
            int dianaMode,
            orthia::Address_type sizeInCommands)
            :
            m_pTextPrinter(pTextPrinter),
            m_dianaMode(dianaMode),
            m_sizeInCommands(sizeInCommands),
            m_currentCommand(0),
            m_bytesIdent(0),
            m_countOfSpacesAfterAddress(1),
            m_printInvalidPages(false),
            m_spacesCount(1),
            m_pDianaPrintContext(0)
        {
            // default parameters are used in windbg plugin
            m_bytesIdent = 25;
            if (m_dianaMode == 8)
            {
                m_bytesIdent = 31;
            }
        }
        int GetDianaMode() const
        {
            return m_dianaMode;
        }
        void SetSpacesCount(int spacesCount)
        {
            m_spacesCount = spacesCount;
            m_writer.SetSpacesCount(spacesCount);
        }
        virtual void PrintCommand(unsigned long long address,
            const orthia::PlatformString_type& bytes,
            const orthia::PlatformString_type& command)
        {
            m_currentBlock.clear();
            m_currentBlock.append(orthia::AddressToString(address, m_dianaMode));
            m_currentBlock.append(m_countOfSpacesAfterAddress, L' ');
            m_currentBlock.append(bytes);

            int count = m_bytesIdent;
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
        virtual bool IsBadByte(orthia::Address_type virtualOffset)
        {
            return false;
        }
        virtual void OnRange(const orthia::VmMemoryRangeInfo& vmRange,
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
            ::DianaMemoryStream stream;
            ctx.pStream = &stream.parent.parent;

            ::DianaParserResult& result = ctx.result;
            ::DianaContext& context = ctx.context;

            m_pDianaPrintContext = &ctx;

            Diana_InitContext(&context, m_dianaMode);
            Diana_InitMemoryStream(&stream, (void*)pDataStart, (size_t)vmRange.size);

            orthia::PlatformString_type temp, binaryData;
            orthia::Address_type virtualOffset = vmRange.address;
            size_t offsetInPage = 0;
            bool prevWasBad = false;
            for (; m_currentCommand < m_sizeInCommands; ++m_currentCommand)
            {
                int iRes = 0;
                if (reportNoData || this->IsBadByte(virtualOffset))
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
                Preprocess(iRes, context, result, virtualOffset, &print, &exit);
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
                            orthia::PlatformString_type dbCommand = L"db";
                            dbCommand.append(m_spacesCount, L' ');
                            PrintCommand(virtualOffset, temp, dbCommand + temp);
                        }
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
                if (print)
                {
                    if (prevWasBad)
                    {
                        PrintCommand(virtualOffset, L"??", L"???");
                    }
                    else
                    {
                        temp = orthia::ToWideString(m_writer.Assign(&result, virtualOffset));
                        binaryData = orthia::ToHexString(pDataStart + offsetInPage, result.iFullCmdSize);

                        PrintCommand(virtualOffset, binaryData, temp);
                    }
                }
                offsetInPage += result.iFullCmdSize;
                virtualOffset += result.iFullCmdSize;

                if (exit)
                {
                    break;
                }
            }

            m_pDianaPrintContext = 0;
        }
    };

    template<class AsmCommandWriterType>
    class CSubrangeMemoryPrinter:public CVmAsmMemoryPrinter<AsmCommandWriterType>
    {
    protected:
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
            if (virtualEndOfCommand <= m_startAddress)
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


