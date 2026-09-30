#include "orthia_commands.h"
#include "orthia_expressions.h"
#include "ui_common.h"
#include "ui_disasm_memory_writer.h"
#include "orthia_match.h"
#include "orthia_model.h"
#include "orthia_external_symbols.h"
#include "orthia_memory_cache.h"

namespace orthia
{

    const int g_maxLinesWithoutSync = 100;

    static bool IsImageReadable(IMemoryReader* reader, const orthia::ModuleInfo& mod)
    {
        if (!mod.size)
        {
            return false;
        }
        char header[2] = { 0, };
        Address_type bytesRead = 0;
        try
        {
            reader->Read(mod.address, sizeof(header), header, &bytesRead, ORTHIA_MR_FLAG_READ_ABSOLUTE, 0, reg_none);
        }
        catch (const std::exception&)
        {
            return false;
        }
        return bytesRead == sizeof(header);
    }
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
        args.progressHandler->ReplyAnyway(args.progressHandler, args.errorText, true);
    }
    void CCommandProcessor::Handle_u(CommandArguments& args)
    {
        const Address_type maxCountOfInstructions = 1000;
        auto resolver = std::make_shared< oui::NameResolverOverWorkplaceItem>(args.item);
        auto range = orthia::CaptureAddressRangeExp(args.parser.GetTokenizer(), resolver);
        auto targetAddress = range.address;
        Address_type countOfInstructions = range.length.value_or(10);
        if (countOfInstructions > maxCountOfInstructions)
        {
            throw std::runtime_error("Length is too big");
        }

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
            countOfInstructions,
            oui::LimitKind::Instructions,
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
        // no module part: search the main module, or every module if the target has none
        Address_type mainModuleAddress = 0;
        if (parts.size() == 1)
        {
            mainModuleAddress = args.item->GetMainModuleAddress();
            parts.insert(parts.begin(), orthia::StringInfo(ORTHIA_TCSTR("*")));
        }
        if (parts.size() != 2)
        {
            throw std::runtime_error("Invalid mask, expected [module!]name: " + orthia::PlatformStringToUtf8(mask));
        }

        std::vector<orthia::ModuleInfo> modules;
        std::vector<orthia::NameInfo> names;
        args.item->GetModules(modules);

        orthia::PlatformString_type text;
        auto dianaMode = args.item->GetDianaMode();
        for (auto& mod : modules)
        {
            if (mainModuleAddress && mod.address != mainModuleAddress)
            {
                continue;
            }
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
            args.model->GetAnalyzer().EnqueueLoadSymbols(args.item, args.progressHandler,
                args.workspaceId, nullptr,
                [this]() {
                });
            return;
        }
        bool moduleFound = false;
        oui::EnumModulesByName(args.item,
            moduleName,
            [&](orthia::ModuleInfo& mod)
        {
            args.model->GetAnalyzer().EnqueueLoadSymbols(args.item, args.progressHandler,
                args.workspaceId, nullptr,
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
                        args.workspaceId, []() {
                        });
                args.model->GetAnalyzer().EnqueueAnalyzePrivateSymbols(args.item, args.progressHandler, mod.address,
                    args.workspaceId, []() {
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

    void CCommandProcessor::Handle_mod_info(CommandArguments& args)
    {
        auto moduleName = orthia::ReadStringOrRaw(args.parser.GetTokenizer().GetTokenizer());
        if (moduleName.empty())
        {
            throw std::runtime_error("Module name expected");
        }
        bool moduleFound = false;
        oui::EnumModulesByName(args.item,
            moduleName,
            [&](orthia::ModuleInfo& mod)  {

            auto pMemoryReader = args.item->CreateMemoryReader();

            // an unresolved dependency owns an address range, but nothing is mapped there
            if (!IsImageReadable(pMemoryReader.get(), mod))
            {
                throw std::runtime_error("No image data for module: " + orthia::PlatformStringToUtf8(mod.name));
            }

            orthia::PlatformString_type line;
            line = ORTHIA_TCSTR("Module: ") + mod.name;
            args.ReplyLine(line);

            line = ORTHIA_TCSTR("Full name: ") + mod.fullName;
            args.ReplyLine(line);

            if (mod.builtInFlags & orthia::ModuleInfo::builtInFlags_moduleTypeElf)
            {
                orthia::PlatformString_type buildIdHex;
                if (!orthia::QueryModuleElfDebugInfo(pMemoryReader.get(), mod, buildIdHex))
                {
                    buildIdHex = ORTHIA_TCSTR("<none>");
                }

                line = ORTHIA_TCSTR("Build ID: ") + buildIdHex;
                args.ReplyLine(line);
            }
            else if (orthia::IsPeModule(mod))
            {
                DIANA_UUID guid = { 0, };
                DI_UINT32 age = 0;
                orthia::PlatformString_type pdbName;
                bool haveDebugInfo = orthia::QueryModulePeDebugInfo(pMemoryReader.get(), mod, guid, age, pdbName);

#ifdef WIN32
                {
                    orthia::CMemoryStorageOfModifiedData storage(pMemoryReader.get());
                    orthia::VmMemoryRangesTargetOverVectorPlain moduleData;
                    storage.ReportRegions(mod.address, mod.size, &moduleData, true);
                    if (!moduleData.m_data.empty())
                    {
                        const wchar_t* version = orthia::QueryModuleVersion((HMODULE)moduleData.m_data.data());
                        if (version)
                        {
                            line = ORTHIA_TCSTR("Version: ") + orthia::PlatformString_type(version);
                            args.ReplyLine(line);
                        }
                    }
                }
#endif

                if (!haveDebugInfo)
                {
                    // no CodeView entry: a zero GUID would look like a real one
                    args.ReplyLine(ORTHIA_TCSTR("Debug GUID: <none>"));
                }
                else
                {
                    line = ORTHIA_TCSTR("Debug GUID: ") + orthia::UUIDToString(guid);
                    args.ReplyLine(line);

                    line = ORTHIA_TCSTR("Debug Age: ") + orthia::ObjectToString(age);
                    args.ReplyLine(line);

                    line = ORTHIA_TCSTR("Pdb name: ") + pdbName;
                    args.ReplyLine(line);
                }
            }
            else
            {
                throw std::runtime_error("Unknown module format: " + orthia::PlatformStringToUtf8(mod.fullName));
            }

            moduleFound = true;
            return false;
        }
        );
        if (!moduleFound)
        {
            throw std::runtime_error("Module not found: " + orthia::PlatformStringToUtf8(moduleName));
        }
    }

    void CCommandProcessor::ExecuteImpl(ThreadPtr_type targetThread,
        oui::OperationPtr_type<ExecuteProgressHandler_type> progressHandler,
        oui::OperationPtr_type<SpecialUICommandHandler_type> uiCommandHandler,
        const orthia::PlatformString_type& text,
        std::shared_ptr<IWorkPlaceItem> item,
        std::shared_ptr<orthia::CProgramModel> model)
    {
        CCommandParser parser;
        CommandArguments args = { progressHandler, parser, item, model, model->GetActiveItemId(), uiCommandHandler };

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
            parser.SetHandler(OUI_TCSTR("modinfo"), [&](CCommandParser& parser) mutable { Handle_mod_info(args);  });
            parser.SetHandler(OUI_TCSTR("cls"), [&](CCommandParser& parser) mutable { uiCommandHandler->Reply(uiCommandHandler, SpecialUICommands::ClearScreen);  });

            parser.Parse(text);
        }
        catch (std::exception& e)
        {
            auto errStr = orthia::Utf8ToPlatformString(e.what());
            // reported by the guard as the final reply, so callers get a failure signal
            args.errorText = oui::String(ORTHIA_TCSTR("Error: ") + errStr);
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
