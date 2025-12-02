#include "orthia_commands.h"
#include "orthia_expressions.h"
#include "ui_common.h"
#include "ui_disasm_memory_writer.h"
namespace orthia
{
    CCommandProcessor::CCommandProcessor()
        :
            m_pool(1)
    {
    }
    void CCommandProcessor::ReportStop(CommandArguments& args)
    {
        args.progressHandler->Reply(args.progressHandler, oui::String(), true);
    }
    void CCommandProcessor::Handle_u(CommandArguments& args)
    {
        std::shared_ptr<ICalcNode> rootNode = std::make_shared<SummNode>(false);
        auto currentNode = rootNode;

        std::vector<Token> tokens;
        Token token;
        int indexOfLength = -1;
        for (; args.parser.GetTokenizer().GetNextToken(&token);)
        {
            if (token.type == Token::ttName)
            {
                auto str = orthia::ReadString(token);
                if (str[0] == 'L')
                {
                    indexOfLength = (int)tokens.size();
                }
            }
            tokens.push_back(token);
        }

        for (int i = 0, size = (int)tokens.size(); i < size; ++i)
        {
            token = tokens[i];
            if (indexOfLength == i)
            {
                break;
            }
            auto result = currentNode->Append(token);
            if (result.newNode)
            {
                currentNode = result.newNode;
            }
        }
        const Address_type maxCountOfLines = 1000;
        Address_type countOfLines = 10;
        orthia::PlatformString_type lengthString;

        if (indexOfLength != -1)
        {
            lengthString = orthia::ReadString(tokens[indexOfLength]);
            countOfLines = oui::CaptureAddress(orthia::PlatformString_type(lengthString.begin() + 1, lengthString.end()));
        }
        if (countOfLines > maxCountOfLines)
        {
            throw std::runtime_error("Length is too big");
        }

        // calc address
        auto resolver = std::make_shared< oui::NameResolverOverWorkplaceItem>(args.item);
        auto targetAddress = orthia::CaptureAddressExp(rootNode, currentNode, token, resolver);

        auto stream = args.item->CreateDisasmStream(targetAddress);
        if (!stream)
        {
            return;
        }

        auto AddTextHandler = [&](const oui::String& text) {

            if (!args.progressHandler->Reply(args.progressHandler, text, false))
            {
                throw std::runtime_error("Request canceled");
            }
        };

        struct DisasmSender :oui::DisasmWriter
        {
            decltype(AddTextHandler) m_handler;

            DisasmSender(decltype(AddTextHandler) handler)
                :
                m_handler(handler)
            {
            }
            void PrintLine(const std::wstring& line) override
            {
                m_handler(line);
            }
            void PrintLine(const std::wstring& line, const oui::TextMarkup& markup, std::shared_ptr<oui::IMultilineViewTag> tag)
            {
                m_handler(line);
            }
        }
        writer(AddTextHandler);
        oui::MemoryPrinter printer(&writer,
            args.item->GetDianaMode(),
            oui::LineIndex(targetAddress, 0),
            countOfLines,
            args.item);

        oui::MemoryPrinter::DianaPrintContext context;
        Diana_InitContext(&context.context, args.item->GetDianaMode());

        context.pStream = stream.get();
        printer.OnStream(&context, oui::LineIndex(targetAddress, 0), false);
    }

    void CCommandProcessor::ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item)
    {
        CCommandParser parser;
        CommandArguments args = { progressHandler, parser, item };

        oui::ScopedGuard reportStopGuard([&]() { ReportStop(args); });

        try
        {
            parser.SetEmptyHandler([&]() {});
            parser.SetHandler(OUI_TCSTR("u"), [=](CCommandParser& parser) mutable { Handle_u(args);  });
            parser.Parse(text);
        }
        catch (std::exception& e)
        {
            auto errStr = orthia::Utf8ToUtf16(e.what());
            args.progressHandler->Reply(args.progressHandler, errStr, false);
        }
    }
    void CCommandProcessor::AsyncExecute(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item)
    {
        m_pool.AddTask([=]() {

            ExecuteImpl(targetThread, progressHandler, text, item);
        });
    }
    bool CCommandProcessor::IsBusy() const
    {
        return m_pool.GetTasksCount();
    }

}
