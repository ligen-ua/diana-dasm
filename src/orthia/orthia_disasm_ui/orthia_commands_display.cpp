#include "orthia_config.h"
#include "orthia_model.h"
#include "ui_common.h"
#include "orthia_common_print.h"

namespace orthia
{

static void WriteFullInvalidRow(CCommandProcessor::CommandArguments& args, Address_type sizeInBytes, int itemSize)
{
}
static void ConvertToTextDPS(void* args_in, const char* pBinary, orthia::PlatformString_type* pText)
{
    CCommandProcessor::CommandArguments& args = *(CCommandProcessor::CommandArguments *)args_in;
    auto address = *(orthia::Address_type*)pBinary;
    *pText = Address64ToString(address);

    auto name = args.item->QueryAddressName(address);
    if (!name.native.empty())
    {
        pText->append(2, ORTHIA_TCHAR(' '));
        pText->append(name.native);
    }
}
void CCommandProcessor::Handle_d(CommandArguments& args, int itemSize, bool dps)
{
    std::shared_ptr<ICalcNode> rootNode = CreateRootNode(&args.parser.GetTokenizer());
    std::vector<Token> tokens;
    const Address_type maxCountOfItems = 100000;
    Address_type countOfItems = 16*8/itemSize;
    int indexOfLength = PrepareTokens(args, tokens, maxCountOfItems, countOfItems);
    auto currentNode = BuildNodes(args, tokens, indexOfLength, rootNode);
    auto resolver = std::make_shared< oui::NameResolverOverWorkplaceItem>(args.item);
    auto targetAddress = orthia::CaptureAddressExp(rootNode, currentNode, tokens.back(), resolver);

    const int columnsCount = dps?1:(16/itemSize);
    std::vector<char> page(columnsCount * 1024);
    Address_type bytesLeft = countOfItems * itemSize;
    auto curAddress = targetAddress;
    auto AddTextHandler = [&](const oui::String& text) {

        args.ReplyLine(text);
    };
    
    struct TextPrinter:ITextPrinter
    {
        decltype(AddTextHandler) m_handler;

        TextPrinter(decltype(AddTextHandler) handler)
            :
            m_handler(handler)
        {
        }

        virtual void PrintLine(const std::wstring& line)
        {
            m_handler(line);
        }
    }textPrinter(AddTextHandler);
    orthia::CVmBinaryMemoryPrinter vmPrinter(&textPrinter, itemSize, args.item->GetDianaMode(), columnsCount);
    if (dps)
    {
        vmPrinter.SetConvertToText(&args, ConvertToTextDPS);
    }
    for (; bytesLeft;)
    {
        auto sizeToRead = std::min<Address_type>(page.size(), bytesLeft);
        auto range = args.item->ReadData(curAddress, sizeToRead);
        if (!range.dataSize)
        {
            range.rangeFlags |= orthia::WorkAddressData::flags_FullInvalid;
            range.dataSize = sizeToRead;
        }
        VmMemoryRangeInfo info;
        bool partialRead = true;
        info.size = range.dataSize;
        info.address = curAddress;
        if (range.rangeFlags & orthia::WorkAddressData::flags_FullInvalid)
        {
            partialRead = false;
        }
        else
        if (range.rangeFlags & orthia::WorkAddressData::flags_FullValid)
        {
            partialRead = false;
            info.flags |= VmMemoryRangeInfo::flags_hasData;
        }
        if (partialRead)
        {
            Address_type rangeStartPos = 0;
            int lastExtraEatenBytes = 0;
            bool rangeInvalid = range.pDataFlags[0] & orthia::WorkAddressData::dataFlags_Invalid;
            for (Address_type i = 1; i <= range.dataSize; ++i)
            {
                bool curInvalid = false;
                bool flush = false;
                if (i == range.dataSize)
                {
                    flush = true;
                }
                else
                {
                    curInvalid = range.pDataFlags[i] & orthia::WorkAddressData::dataFlags_Invalid;
                }
                if (flush || curInvalid != rangeInvalid)
                {
                    VmMemoryRangeInfo info;
                    info.size = i - rangeStartPos;
                    info.address = curAddress + rangeStartPos;
                    if (info.size)
                    {
                        if (rangeInvalid)
                        {
                            info.flags = 0;
                            info.size -= lastExtraEatenBytes;
                            vmPrinter.OnRange(info, 0);
                            lastExtraEatenBytes = (int)vmPrinter.GetExtraEatenBytes();
                        }
                        else
                        {
                            info.flags |= VmMemoryRangeInfo::flags_hasData;
                            info.size -= lastExtraEatenBytes;
                            vmPrinter.OnRange(info, range.pDataStart + rangeStartPos);
                            lastExtraEatenBytes = (int)vmPrinter.GetExtraEatenBytes();
                        }
                    }
                    rangeInvalid = curInvalid;
                    rangeStartPos = i;
                }
            }
        }
        else
        {
            vmPrinter.OnRange(info, range.pDataStart);
        }
        bytesLeft -= sizeToRead;    
    }
    vmPrinter.Finish();
}

}