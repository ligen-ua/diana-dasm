#include "orthia_config.h"
#include "orthia_model.h"
#include "ui_common.h"

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


void CCommandProcessor::Handle_threads(CommandArguments& args)
{
    auto rawArgs = args.parser.GetTokenizer().GetTokenizer().GetNextRawString();
    orthia::TrimStringAllWhiteSpace(rawArgs);

    if (!rawArgs.empty())
    {
        throw std::runtime_error("Argument not supported: " + rawArgs);
    }

    auto proc = args.item->GetAssociatedProcess();
    if (!proc)
    {
        throw std::runtime_error("No process - no threads");
    }

    int dianaMode = args.item->GetDianaMode();

    // print header
    int addressTextSize = (int)orthia::AddressToString(0, dianaMode).size();

    auto [error, enumerator] = proc->CreateThreadEnumerator();
    if (error)
    {
        throw orthia::CWin32Exception("Can't enum threads", error);
    }
    if (!enumerator)
    {
        throw std::runtime_error("Can't enum threads");
    }

    orthia::PlatformString_type line;
    std::vector<orthia::Address_type> stack;
    for (;;)
    {
        auto [error, thread] = enumerator->GetNextThread();
        if (error)
        {
            throw orthia::CWin32Exception("Can't enum threads", error);
        }
        if (!thread)
        {
            break;
        }
        oui::ThreadInfo info;
        thread->GetInfo(info);

        line = ORTHIA_TCSTR("THREAD ") + orthia::AddressToString(info.tid, dianaMode);
        args.ReplyLine(line);

        line = ORTHIA_TCSTR("Start address: ") + QueryAddressNameDef(args.item, info.startAddress, dianaMode).native;
        args.ReplyLine(line);

        line = ORTHIA_TCSTR("Creation time: ") + info.startTime.GUI_QueryConvertedToLocal();
        args.ReplyLine(line);

        stack.clear();
        thread->CaptureStack(20, stack);
        line = ORTHIA_TCSTR("Stack:");
        args.ReplyLine(line);

        for (auto& address: stack)
        {
            line.clear();
            line.resize(6, ORTHIA_TCHAR(' '));
            auto name = QueryAddressNameDef(args.item, address, dianaMode);
            line += name.native;
            args.ReplyLine(line);
        }
        line.clear();
        args.ReplyLine(line);

        line.clear();
        args.Sync();
    }
}

void CCommandProcessor::Handle_lm(CommandArguments& args)
{
    auto rawArgs = args.parser.GetTokenizer().GetTokenizer().GetNextRawString();
    orthia::TrimStringAllWhiteSpace(rawArgs);

    if (!rawArgs.empty())
    {
        throw std::runtime_error("Argument not supported: " + rawArgs);
    }
        
    std::vector<orthia::ModuleInfo> modules;
    args.item->GetModules(modules);

    orthia::PlatformString_type line;
    int dianaMode = args.item->GetDianaMode();

    // print header
    int addressTextSize = (int)orthia::AddressToString(0, dianaMode).size();

    orthia::PlatformString_type columnStart(ORTHIA_TCSTR("start"));
    orthia::PlatformString_type columnEnd(ORTHIA_TCSTR("end"));
    orthia::PlatformString_type columnName(ORTHIA_TCSTR("module name")); 
    
    orthia::PlatformString_type column;
    column = columnStart;
    column.resize(addressTextSize + 2, ORTHIA_TCHAR(' '));
    line = column;

    column = columnEnd;
    column.resize(addressTextSize + 3, ORTHIA_TCHAR(' '));
    line += column;

    column = columnName;
    line += column;
    args.ReplyLine(line);
 
    for (auto& mod : modules)
    {
        line = orthia::AddressToString(mod.address, dianaMode);
        line.append(2, ORTHIA_TCHAR(' '));
        line += orthia::AddressToString(mod.address + mod.size, dianaMode);
        line.append(3, ORTHIA_TCHAR(' '));
        line += mod.name;

        args.ReplyLine(line);
        line.clear();
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