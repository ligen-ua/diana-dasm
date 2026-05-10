#include "orthia_commands.h"
#include "orthia_expressions.h"
#include "ui_common.h"
#include "ui_disasm_memory_writer.h"
#include "orthia_match.h"
#include "orthia_model.h"

namespace orthia
{

    const int g_maxLinesWithoutSync = 100;
    CCommandProcessor::RequestCanceledException::RequestCanceledException()
        :   
            std::runtime_error("Request canceled")
    {
    }
    void CCommandProcessor::CommandArguments::ReplyLine(const oui::String& text)
    {
        if (!progressHandler->Reply(progressHandler, text, false))
        {
            throw RequestCanceledException();
        }
        if (++linesWithoutSync > g_maxLinesWithoutSync)
        {
            Sync();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    void CCommandProcessor::CommandArguments::Sync()
    {
        if (!progressHandler->Sync())
        {
            throw RequestCanceledException();
        }
        linesWithoutSync = 0;
    }

    CCommandProcessor::CCommandProcessor()
        :
            m_pool(1)
    {
    }
    void CCommandProcessor::ReportStop(CommandArguments& args)
    {
        args.progressHandler->ReplyAnyway(args.progressHandler, oui::String(), true);
    }
    int CCommandProcessor::PrepareTokens(CommandArguments& args, std::vector<Token> & tokens, const Address_type maxCountOfItems, Address_type & countOfItems)
    {
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
        if (tokens.empty())
        {
            throw orthia::NoTokenError();
        }


        if (indexOfLength != -1)
        {
            orthia::PlatformString_type lengthString = orthia::ReadString(tokens[indexOfLength]);
            countOfItems = oui::CaptureAddress(orthia::PlatformString_type(lengthString.begin() + 1, lengthString.end()));
        }
        if (countOfItems > maxCountOfItems)
        {
            throw std::runtime_error("Length is too big");
        }
        return indexOfLength;
    }
    std::shared_ptr<ICalcNode> CCommandProcessor::BuildNodes(CommandArguments& args, std::vector<Token>& tokens, int indexOfLength, std::shared_ptr<ICalcNode> currentNode)
    {        
         // calc address
         for (int i = 0, size = (int)tokens.size(); i < size; ++i)
         {
             if (indexOfLength == i)
             {
                 break;
             }
             currentNode = AppendToken(currentNode, tokens[i]);
         }
         return currentNode;
    }
    void CCommandProcessor::Handle_u(CommandArguments& args)
    {
        std::shared_ptr<ICalcNode> rootNode = CreateRootNode(&args.parser.GetTokenizer());
        std::vector<Token> tokens;
        const Address_type maxCountOfLines = 1000;
        Address_type countOfLines = 10;
        int indexOfLength = PrepareTokens(args, tokens, maxCountOfLines, countOfLines);
        auto currentNode = BuildNodes(args, tokens, indexOfLength, rootNode);
        auto resolver = std::make_shared< oui::NameResolverOverWorkplaceItem>(args.item);
        auto targetAddress = orthia::CaptureAddressExp(rootNode, currentNode, tokens.back(), resolver);

        auto stream = args.item->CreateDisasmStream(targetAddress);
        if (!stream)
        {
            return;
        }

        auto AddTextHandler = [&](const oui::String& text) {

            args.ReplyLine(text);
        };

        struct DisasmSender :oui::DisasmWriter
        {
            decltype(AddTextHandler) m_handler;

            DisasmSender(decltype(AddTextHandler) handler)
                :
                m_handler(handler)
            {
            }
            void PrintLine(const orthia::PlatformString_type& line) override
            {
                m_handler(line);
            }
            void PrintLine(const orthia::PlatformString_type& line, const oui::TextMarkup& markup, std::shared_ptr<oui::IMultilineViewTag> tag)
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

    void CCommandProcessor::Handle_x(CommandArguments& args)
    {
        auto mask = orthia::ReadStringOrRaw(args.parser.GetTokenizer().GetTokenizer());
        auto maskDowncase = orthia::Downcase(mask);

        std::vector<StringInfo> parts;
        orthia::SplitString(maskDowncase, orthia::StringInfo(ORTHIA_TCSTR("!")), &parts);
        if (parts.size() != 2)
        {
            return;
        }

        std::vector<orthia::ModuleInfo> modules;
        std::vector<orthia::NameInfo> names;
        args.item->GetModules(modules);

        orthia::PlatformString_type text;
        auto dianaMode = args.item->GetDianaMode();
        for (auto& mod : modules)
        {
            auto modDowncased = orthia::Downcase(mod.name);
            bool match = utils::match(parts[0].ToString(), modDowncased);
            if (!match)
            {
                orthia::PlatformString_type extension;
                orthia::GetExtensionOfFile(modDowncased, &extension);
                if (!extension.empty())
                {
                    modDowncased.erase(modDowncased.size() - extension.size() - 1);
                    match = utils::match(parts[0].ToString(), modDowncased);
                }
            }
            if (match)
            {
                const int c_pageSize = 5000;
                orthia::NameSelectionKey key;
                key.excludeImports = true;

                for (;;)
                {
                    args.item->QueryNames(mod.address, key, c_pageSize, names);
                    if (names.empty())
                    {
                        break;
                    }

                    for (auto& name : names)
                    {
                        auto nameDowncased = orthia::Downcase(name.name.native);
                        if (utils::match(parts[1].ToString(), nameDowncased))
                        {
                            text.clear();
                            text.append(orthia::AddressToString(name.address, dianaMode));
                            text.append(ORTHIA_TCSTR("  "));
                            text.append(mod.name);
                            text.append(ORTHIA_TCSTR("!"));
                            text.append(name.name.native);
                            args.ReplyLine(text);
                        }
                    }
                    key.flags |= key.flags_ContinueFrom;
                    key.address = names.back().address;
                    key.continueMarkNameFlag = names.back().flags;
                }
            }
        }
    }

    void CCommandProcessor::Handle_reload(CommandArguments& args)
    {
        auto moduleName = orthia::ReadStringOrRaw(args.parser.GetTokenizer().GetTokenizer());
        if (moduleName.empty())
        {
            args.model->GetAnalyzer().EnqueueLoadSymbols(args.item, args.progressHandler, nullptr,
                [this]() {
                });
            return;
        }
        bool moduleFound = false;
        oui::EnumModulesByName(args.item,
            moduleName,
            [&](orthia::ModuleInfo& mod)
        {
            args.model->GetAnalyzer().EnqueueLoadSymbols(args.item, args.progressHandler, nullptr,
            [this]() {
                },
             mod.address);
            moduleFound = true;
            return false;
        });
        if (!moduleFound)
        {
            throw std::runtime_error("Module not found: " + orthia::PlatformStringToUtf8(moduleName));
        }
    }

    void CCommandProcessor::Handle_analyze(CommandArguments& args)
    {
        auto moduleName = orthia::ReadStringOrRaw(args.parser.GetTokenizer().GetTokenizer());
        if (moduleName.empty())
        {
            throw std::runtime_error("Module name expected");
        }
        bool moduleFound = false;
        oui::EnumModulesByName(args.item,
            moduleName,
            [&](orthia::ModuleInfo& mod)
            {
                args.model->GetAnalyzer().EnqueueAnalyze(args.item, args.progressHandler, mod.address,
                        []() {
                        });
                args.model->GetAnalyzer().EnqueueAnalyzePrivateSymbols(args.item, args.progressHandler, mod.address,
                    []() {
                });
                moduleFound = true;
                return false;
            }
        );
        if (!moduleFound)
        {
            throw std::runtime_error("Module not found: " + orthia::PlatformStringToUtf8(moduleName));
        }
    }

    void CCommandProcessor::Handle_symfix(CommandArguments& args)
    {
        auto text = orthia::ReadStringOrRaw(args.parser.GetTokenizer().GetTokenizer());
        if (text.empty())
        {
            throw std::runtime_error("File paths separated by semicolon are expected");
        }
        args.model->GetConfig()->SetSymbolsFolders(text);
        orthia::PlatformString_type line;
        line = ORTHIA_TCSTR("Symbols directories: ") + text;
        args.ReplyLine(line);
    }

    void CCommandProcessor::ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model)
    {
        CCommandParser parser;
        CommandArguments args = { progressHandler, parser, item, model };

        oui::ScopedGuard reportStopGuard([&]() { ReportStop(args); });

        try
        {
            parser.SetEmptyHandler([&]() {});
            parser.SetHandler(OUI_TCSTR("threads"), [&](CCommandParser& parser) mutable { Handle_threads(args);  });
            parser.SetHandler(OUI_TCSTR("u"), [&](CCommandParser& parser) mutable { Handle_u(args);  });
            parser.SetHandler(OUI_TCSTR("x"), [&](CCommandParser& parser) mutable { Handle_x(args);  });
            parser.SetHandler(OUI_TCSTR("db"), [&](CCommandParser& parser) mutable { Handle_d(args, 1);  });
            parser.SetHandler(OUI_TCSTR("dw"), [&](CCommandParser& parser) mutable { Handle_d(args, 2);  });
            parser.SetHandler(OUI_TCSTR("dd"), [&](CCommandParser& parser) mutable { Handle_d(args, 4);  });
            parser.SetHandler(OUI_TCSTR("dq"), [&](CCommandParser& parser) mutable { Handle_d(args, 8);  });
            parser.SetHandler(OUI_TCSTR("dp"), [&](CCommandParser& parser) mutable { Handle_d(args, args.item->GetDianaMode());  });
            parser.SetHandler(OUI_TCSTR("dps"), [&](CCommandParser& parser) mutable { Handle_d(args, args.item->GetDianaMode(), true);  });
            parser.SetHandler(OUI_TCSTR("lm"), [&](CCommandParser& parser) mutable { Handle_lm(args);  });
            parser.SetHandler(OUI_TCSTR(".reload"), [&](CCommandParser& parser) mutable { Handle_reload(args);  });
            parser.SetHandler(OUI_TCSTR(".analyze"), [&](CCommandParser& parser) mutable { Handle_analyze(args);  });
            parser.SetHandler(OUI_TCSTR(".symfix"), [&](CCommandParser& parser) mutable { Handle_symfix(args);  });
            parser.SetHandler(OUI_TCSTR("cls"), [&](CCommandParser& parser) mutable { uiCommandHandler->Reply(uiCommandHandler, SpecialUICommands::ClearScreen);  });

            parser.Parse(text);
        }
        catch (std::exception& e)
        {
            auto errStr = orthia::Utf8ToPlatformString(e.what());
            args.progressHandler->ReplyAnyway(args.progressHandler, ORTHIA_TCSTR("Error: ") + errStr, false);
            return;
        }
    }
    void CCommandProcessor::AsyncExecute(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model)
    {
        m_pool.AddTask([=]() {

            ExecuteImpl(targetThread, progressHandler, uiCommandHandler, text, item, model);
        });
    }
    bool CCommandProcessor::IsBusy() const
    {
        return m_pool.GetTasksCount();
    }

}
