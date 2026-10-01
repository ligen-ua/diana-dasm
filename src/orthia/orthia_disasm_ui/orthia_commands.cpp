#include "orthia_commands.h"
#include "orthia_expressions.h"
#include "ui_common.h"
#include "ui_disasm_memory_writer.h"
#include "orthia_match.h"
#include "orthia_model.h"
#include "orthia_external_symbols.h"
#include "orthia_image_source.h"
#include "orthia_memory_cache.h"
#include "orthia_databases.h"
#include "orthia_helpers.h"
#include <ctime>
#include <set>

namespace orthia
{

    const int g_maxLinesWithoutSync = 100;

    static void CheckImageReadable(IMemoryReader* reader, const orthia::ModuleInfo& mod)
    {
        if (!orthia::IsModuleImageReadable(reader, mod))
        {
            throw std::runtime_error("No image data for module: " + orthia::PlatformStringToUtf8(mod.name));
        }
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
    // commands that read the target: the UI and --cmd let only the data folder commands run without one
    static void NeedItem(const CCommandProcessor::CommandArguments& args)
    {
        if (!args.item)
        {
            throw std::runtime_error("No active workspace");
        }
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
        if (!countOfInstructions)
        {
            return;
        }

        // ReadData, not CreateDisasmStream: it knows which bytes are readable, so unmapped memory
        // is reported instead of printing nothing (file) or disassembling zeroes (process)
        const Address_type maxInstructionSize = 15;
        const Address_type sizeToRead = countOfInstructions * maxInstructionSize;
        auto data = args.item->ReadData(targetAddress, sizeToRead);
        Address_type validBytes = 0;
        if (!(data.rangeFlags & orthia::WorkAddressData::flags_FullInvalid) && data.pDataStart)
        {
            validBytes = data.dataSize;
            if (data.pDataFlags)
            {
                for (validBytes = 0; validBytes < data.dataSize; ++validBytes)
                {
                    if (data.pDataFlags[validBytes] & orthia::WorkAddressData::dataFlags_Invalid)
                    {
                        break;
                    }
                }
            }
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

        oui::LineIndex stopAddress(targetAddress, 0);
        if (validBytes)
        {
            ::DianaMemoryStream stream;
            Diana_InitMemoryStreamEx2(&stream, (void*)data.pDataStart, (DIANA_SIZE_T)validBytes, 0, 0);

            oui::MemoryPrinter::DianaPrintContext context;
            Diana_InitContext(&context.context, args.item->GetDianaMode());
            context.pStream = &stream.parent.parent;
            printer.OnStream(&context, oui::LineIndex(targetAddress, 0), false);

            // undecodable bytes are not counted as commands, so fewer can be printed from readable memory too
            if (printer.GetPrintedCommands() >= countOfInstructions || validBytes >= sizeToRead)
            {
                return;
            }
            stopAddress = printer.GetStopAddress();
        }
        // like WinDbg: mark where the readable memory ends and fail
        printer.PrintCommand(oui::LineIndex(stopAddress.GetIndex(), 0), ORTHIA_TCSTR("??"), ORTHIA_TCSTR("???"));
        throw std::runtime_error("Memory access error at " + orthia::PlatformStringToUtf8(
            orthia::AddressToString(stopAddress.GetIndex(), args.item->GetDianaMode())));
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
                // an export also has a PDB record with the same name and address; exports come first
                std::set<std::pair<Address_type, orthia::PlatformString_type>> printed;

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
                        if (utils::match(parts[1].ToString(), nameDowncased) &&
                            printed.emplace(name.address, name.name.native).second)
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
                // checked here: a failure in the background analysis cannot fail the command
                CheckImageReadable(args.item->CreateMemoryReader().get(), mod);

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

            orthia::PlatformString_type line;
            line = ORTHIA_TCSTR("Module: ") + mod.name;
            args.ReplyLine(line);

            line = ORTHIA_TCSTR("Full name: ") + mod.fullName;
            args.ReplyLine(line);

            auto imageSource = orthia::ModuleImageSourceText(mod);
            if (!imageSource.empty())
            {
                // said before the failure below, so a stale dependency explains itself
                line = ORTHIA_TCSTR("Image: ") + imageSource;
                args.ReplyLine(line);
            }

            CheckImageReadable(pMemoryReader.get(), mod);

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

    static const PlatformString_type g_databaseCommand = ORTHIA_TCSTR(".database");

    // Splits a command line on whitespace; double quotes keep a name with spaces together.
    static std::vector<PlatformString_type> SplitCommandWords(const PlatformString_type& text)
    {
        std::vector<PlatformString_type> words;
        PlatformString_type word;
        bool inWord = false;
        bool quoted = false;
        for (auto ch : text)
        {
            if (ch == '"')
            {
                quoted = !quoted;
                inWord = true;
                continue;
            }
            if (!quoted && (ch == ' ' || ch == '\t'))
            {
                if (inWord)
                {
                    words.push_back(word);
                    word.clear();
                    inWord = false;
                }
                continue;
            }
            word.push_back(ch);
            inWord = true;
        }
        if (quoted)
        {
            throw std::runtime_error("Unterminated quote");
        }
        if (inWord)
        {
            words.push_back(word);
        }
        return words;
    }

    bool CCommandProcessor::IsTargetless(const PlatformString_type& text)
    {
        auto begin = text.find_first_not_of(ORTHIA_TCSTR(" \t"));
        if (begin == PlatformString_type::npos)
        {
            return false;
        }
        auto end = text.find_first_of(ORTHIA_TCSTR(" \t"), begin);
        auto word = end == PlatformString_type::npos ? text.substr(begin) : text.substr(begin, end - begin);
        return word == g_databaseCommand;
    }

    static PlatformString_type PadRight(PlatformString_type text, size_t width)
    {
        if (text.size() < width)
        {
            text.append(width - text.size(), ' ');
        }
        return text;
    }

    static PlatformString_type FormatSize(unsigned long long size)
    {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        double value = (double)size;
        int unit = 0;
        while (value >= 1024 && unit < 4)
        {
            value /= 1024;
            ++unit;
        }
        char buffer[32];
        if (unit == 0)
        {
            snprintf(buffer, sizeof(buffer), "%llu B", size);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%.1f %s", value, units[unit]);
        }
        return Utf8ToPlatformString(buffer);
    }

    static PlatformString_type FormatFileTime(std::filesystem::file_time_type time)
    {
        if (time == std::filesystem::file_time_type::min())
        {
            return ORTHIA_TCSTR("?");
        }
        // C++17 has no clock_cast: go through the distance from now
        auto systemTime = std::chrono::system_clock::now() +
            std::chrono::duration_cast<std::chrono::system_clock::duration>(time - std::filesystem::file_time_type::clock::now());
        std::time_t value = std::chrono::system_clock::to_time_t(systemTime);
        tm local = { 0 };
#ifdef DIANA_HAS_WIN32
        localtime_s(&local, &value);
#else
        localtime_r(&value, &local);
#endif
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
        return Utf8ToPlatformString(buffer);
    }

    static PlatformString_type DescribeDatabase(const DatabaseInfo& info)
    {
        if (info.originalName.empty() || info.originalName == info.folderName)
        {
            return info.folderName;
        }
        return info.folderName + ORTHIA_TCSTR(" ") + info.originalName;
    }

    void CCommandProcessor::Handle_database_list(CommandArguments& args)
    {
        auto config = args.model->GetConfig();
        auto all = EnumerateDatabases(*config);
        auto openFolders = args.model->QueryOpenDatabaseFolders();

        auto dataFolder = config->GetDataFolder();
        EraseLastSlash(dataFolder);
        args.ReplyLine(ORTHIA_TCSTR("Data folder: ") + dataFolder);
        if (all.empty())
        {
            args.ReplyLine(ORTHIA_TCSTR("No databases"));
            return;
        }
        const size_t widths[] = { 12, 16, 10, 14, 8 };
        auto formatRow = [&](const PlatformString_type* fields, const PlatformString_type& name) {
            PlatformString_type line;
            for (size_t i = 0; i < sizeof(widths) / sizeof(widths[0]); ++i)
            {
                line += PadRight(fields[i], widths[i]) + ORTHIA_TCSTR("  ");
            }
            return line + name;
        };
        const PlatformString_type header[] = { ORTHIA_TCSTR("Database"), ORTHIA_TCSTR("Modified"), ORTHIA_TCSTR("Size"),
            ORTHIA_TCSTR("State"), ORTHIA_TCSTR("Comments") };
        args.ReplyLine(formatRow(header, ORTHIA_TCSTR("Name")));
        for (const auto& info : all)
        {
            PlatformString_type id;
            if (info.kind == DatabaseInfo::Kind::File)
            {
                id = info.folderName.substr(0, 12);
            }
            else if (info.pid)
            {
                orthia::ObjectToString_t(info.pid, id);
                id = ORTHIA_TCSTR("pid ") + id;
            }
            else
            {
                id = ORTHIA_TCSTR("proc");
            }

            PlatformString_type state = ORTHIA_TCSTR("?");
            PlatformString_type comments = ORTHIA_TCSTR("?");
            if (info.pendingDelete)
            {
                state = ORTHIA_TCSTR("deleting");
            }
            else if (!info.hasDataDb)
            {
                state = ORTHIA_TCSTR("empty");
            }
            else
            {
                auto details = QueryDatabaseDetails(*config, info);
                if (details.mainModuleKnown)
                {
                    state = (details.mainModuleFlags & ModuleInfo::flags_analyzeDone) ? ORTHIA_TCSTR("analyzed") : ORTHIA_TCSTR("quick");
                    if (details.mainModuleFlags & ModuleInfo::flags_symbolsLoaded)
                    {
                        state += ORTHIA_TCSTR("+symbols");
                    }
                }
                if (details.commentsKnown)
                {
                    orthia::ObjectToString_t(details.comments, comments);
                }
            }

            PlatformString_type name = info.originalName.empty() ? info.folderName : info.originalName;
            const PlatformString_type fields[] = { id, FormatFileTime(info.lastWrite), FormatSize(info.sizeBytes), state, comments };
            PlatformString_type line = formatRow(fields, name);
            if (openFolders.count(info.folder))
            {
                line += ORTHIA_TCSTR("  (open)");
            }
            args.ReplyLine(line);
        }
    }

    void CCommandProcessor::Handle_database_delete(CommandArguments& args, const std::vector<PlatformString_type>& selectors)
    {
        if (selectors.empty())
        {
            throw std::runtime_error("Database expected: a sha1 prefix, a pid or a file name");
        }
        auto config = args.model->GetConfig();
        auto fileSystem = args.model->GetFileSystem();
        auto all = EnumerateDatabases(*config);

        // the file system that opens targets, so the hash is the one the folder is named by
        auto hashFile = [&](const PlatformString_type& name) -> PlatformString_type {
            int error = 0;
            oui::String fullName;
            std::tie(error, fullName) = fileSystem->SyncGetFullPathName(name);
            std::shared_ptr<oui::IFile2> file;
            if (!error)
            {
                std::tie(error, file) = fileSystem->SyncOpenFile(oui::FileUnifiedId(fullName));
            }
            if (!file)
            {
                throw std::runtime_error("Can't open file: " + PlatformStringToUtf8(name));
            }
            auto hash = CalcSha1(file, args.progressHandler);
            return orthia::ToHexString(hash.data(), hash.size());
        };
        auto selected = ResolveDatabaseSelectors(all, selectors, hashFile);

        auto openFolders = args.model->QueryOpenDatabaseFolders();
        for (const auto& info : selected)
        {
            auto it = openFolders.find(info.folder);
            if (it != openFolders.end())
            {
                throw std::runtime_error("Database " + PlatformStringToUtf8(info.folderName) + " is open as " +
                    PlatformStringToUtf8(it->second) + ", close it first");
            }
        }

        std::string lastError;
        size_t failed = 0;
        for (const auto& info : selected)
        {
            try
            {
                DeleteDatabaseFolder(info);
                args.ReplyLine(ORTHIA_TCSTR("Deleted ") + DescribeDatabase(info));
            }
            catch (RequestCanceledException&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                lastError = e.what();
                ++failed;
                if (selected.size() > 1)
                {
                    args.ReplyLine(ORTHIA_TCSTR("Error: ") + Utf8ToPlatformString(lastError));
                }
            }
        }
        if (failed && selected.size() == 1)
        {
            throw std::runtime_error(lastError);
        }
        if (failed)
        {
            throw std::runtime_error(std::to_string(failed) + " of " + std::to_string(selected.size()) + " databases were not deleted");
        }
    }

    void CCommandProcessor::Handle_database_cleanup(CommandArguments& args)
    {
        auto config = args.model->GetConfig();
        auto processSystem = args.model->GetProcessSystem();
        auto fileSystem = args.model->GetFileSystem();
        auto all = EnumerateDatabases(*config);
        auto openFolders = args.model->QueryOpenDatabaseFolders();

        // through the same providers that open processes and name their folders
        auto entries = SelectDatabasesForCleanup(all, openFolders, std::filesystem::file_time_type::clock::now(),
            [&](const DatabaseInfo& info) { return QueryProcessState(*processSystem, *fileSystem, info); });
        if (entries.empty())
        {
            args.ReplyLine(ORTHIA_TCSTR("Nothing to clean up"));
            return;
        }

        int removed = 0;
        unsigned long long freed = 0;
        for (const auto& entry : entries)
        {
            try
            {
                DeleteDatabaseFolder(entry.info);
                args.ReplyLine(ORTHIA_TCSTR("Removed ") + DescribeDatabase(entry.info) + ORTHIA_TCSTR(": ") + entry.reason);
                ++removed;
                freed += entry.info.sizeBytes;
            }
            catch (RequestCanceledException&)
            {
                throw;
            }
            catch (std::exception& e)
            {
                // another instance may still use it: not an error of this command
                args.ReplyLine(ORTHIA_TCSTR("Kept ") + DescribeDatabase(entry.info) + ORTHIA_TCSTR(": ") + Utf8ToPlatformString(e.what()));
            }
        }
        PlatformString_type countText;
        orthia::ObjectToString_t(removed, countText);
        args.ReplyLine(ORTHIA_TCSTR("Removed ") + countText + ORTHIA_TCSTR(" folder(s), freed ") + FormatSize(freed));
    }

    void CCommandProcessor::Handle_database(CommandArguments& args)
    {
        const char* usage = "Usage: .database list | .database delete <sha1 prefix|pid|file> ... | .database cleanup";
        auto words = SplitCommandWords(args.text);
        if (words.size() < 2)
        {
            throw std::runtime_error(usage);
        }
        const auto& subcommand = words[1];
        std::vector<PlatformString_type> rest(words.begin() + 2, words.end());
        if (subcommand == ORTHIA_TCSTR("list") && rest.empty())
        {
            Handle_database_list(args);
            return;
        }
        if (subcommand == ORTHIA_TCSTR("delete"))
        {
            Handle_database_delete(args, rest);
            return;
        }
        if (subcommand == ORTHIA_TCSTR("cleanup") && rest.empty())
        {
            Handle_database_cleanup(args);
            return;
        }
        throw std::runtime_error(usage);
    }

    void CCommandProcessor::Handle_sections(CommandArguments& args)
    {
        const char* usage = "Usage: sections [-v] <module|address>";
        auto words = SplitCommandWords(args.text);
        bool verbose = false;
        size_t first = 1;
        if (words.size() > first && words[first] == ORTHIA_TCSTR("-v"))
        {
            verbose = true;
            ++first;
        }
        if (words.size() <= first)
        {
            throw std::runtime_error(usage);
        }
        // an expression may contain spaces
        PlatformString_type target;
        for (size_t i = first; i < words.size(); ++i)
        {
            target += (i == first ? ORTHIA_TCSTR("") : ORTHIA_TCSTR(" ")) + words[i];
        }

        orthia::ModuleInfo module;
        bool moduleFound = false;
        oui::EnumModulesByName(args.item, target, [&](orthia::ModuleInfo& mod) {
            module = mod;
            moduleFound = true;
            return false;
        });
        if (!moduleFound)
        {
            // not a module name: the module that contains the address
            Address_type address = 0;
            try
            {
                address = oui::CaptureAddressExp(target, args.item);
            }
            catch (std::exception&)
            {
                throw std::runtime_error("Module not found: " + orthia::PlatformStringToUtf8(target));
            }
            std::vector<orthia::ModuleInfo> modules;
            args.item->GetModules(modules);
            for (auto& mod : modules)
            {
                if (address >= mod.address && address <= mod.lastValidAddress)
                {
                    module = mod;
                    moduleFound = true;
                    break;
                }
            }
            if (!moduleFound)
            {
                throw std::runtime_error("No module at " + orthia::PlatformStringToUtf8(
                    orthia::AddressToString(address, args.item->GetDianaMode())));
            }
        }

        args.ReplyLine(ORTHIA_TCSTR("Module: ") + module.name);
        CheckImageReadable(args.item->CreateMemoryReader().get(), module);

        orthia::ImageSections result;
        args.item->QuerySections(module.address, result);
        if (result.sections.empty())
        {
            throw std::runtime_error("No sections found in module: " + orthia::PlatformStringToUtf8(module.name));
        }
        if (result.segments)
        {
            // said first: the rows below are segments, not sections
            args.ReplyLine(ORTHIA_TCSTR("No section headers: ") + result.reason);
            args.ReplyLine(ORTHIA_TCSTR("Showing program headers (segments) from memory"));
        }
        else if (!result.source.empty())
        {
            args.ReplyLine(ORTHIA_TCSTR("Section headers: ") + result.source);
        }

        size_t nameWidth = 4;
        size_t sizeWidth = 4;
        for (const auto& section : result.sections)
        {
            nameWidth = std::max(nameWidth, section.name.size());
            sizeWidth = std::max(sizeWidth, orthia::ToWideStringAsHex_Short(section.size).size());
        }
        auto dianaMode = args.item->GetDianaMode();
        size_t addressWidth = orthia::AddressToString(0, dianaMode).size();
        args.ReplyLine(PadRight(ORTHIA_TCSTR("Name"), nameWidth) + ORTHIA_TCSTR("  ") +
            PadRight(ORTHIA_TCSTR("Address"), addressWidth) + ORTHIA_TCSTR("  ") +
            PadRight(ORTHIA_TCSTR("Size"), sizeWidth) + ORTHIA_TCSTR("  Flags"));
        for (const auto& section : result.sections)
        {
            // an ELF section that is not loaded has no address
            PlatformString_type address = section.address ? orthia::AddressToString(section.address, dianaMode) : ORTHIA_TCSTR("-");
            args.ReplyLine(PadRight(section.name, nameWidth) + ORTHIA_TCSTR("  ") +
                PadRight(address, addressWidth) + ORTHIA_TCSTR("  ") +
                PadRight(orthia::ToWideStringAsHex_Short(section.size), sizeWidth) + ORTHIA_TCSTR("  ") +
                section.flagsShort);
            if (verbose)
            {
                for (const auto& attribute : section.attributes)
                {
                    // the unnamed one is the decoded flags, a list with a trailing space
                    auto value = attribute.second;
                    value.erase(value.find_last_not_of(ORTHIA_TCSTR(" ")) + 1);
                    args.ReplyLine(ORTHIA_TCSTR("    ") +
                        (attribute.first.empty() ? value : attribute.first + ORTHIA_TCSTR(": ") + value));
                }
            }
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
        args.text = text;

        oui::ScopedGuard reportStopGuard([&]() { ReportStop(args); });

        try
        {
            parser.SetEmptyHandler([&]() {});
            parser.SetHandler(OUI_TCSTR("threads"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_threads(args);  });
            parser.SetHandler(OUI_TCSTR("u"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_u(args);  });
            parser.SetHandler(OUI_TCSTR("x"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_x(args);  });
            parser.SetHandler(OUI_TCSTR("db"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, 1);  });
            parser.SetHandler(OUI_TCSTR("dw"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, 2);  });
            parser.SetHandler(OUI_TCSTR("dd"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, 4);  });
            parser.SetHandler(OUI_TCSTR("dq"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, 8);  });
            parser.SetHandler(OUI_TCSTR("dp"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, args.item->GetDianaMode());  });
            parser.SetHandler(OUI_TCSTR("dps"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_d(args, args.item->GetDianaMode(), true);  });
            parser.SetHandler(OUI_TCSTR("lm"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_lm(args);  });
            parser.SetHandler(OUI_TCSTR(".reload"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_reload(args);  });
            parser.SetHandler(OUI_TCSTR(".analyze"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_analyze(args);  });
            parser.SetHandler(OUI_TCSTR(".symfix"), [&](CCommandParser& parser) mutable { Handle_symfix(args);  });
            parser.SetHandler(OUI_TCSTR("modinfo"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_mod_info(args);  });
            parser.SetHandler(OUI_TCSTR("sections"), [&](CCommandParser& parser) mutable { NeedItem(args); Handle_sections(args);  });
            parser.SetHandler(g_databaseCommand, [&](CCommandParser& parser) mutable { Handle_database(args);  });
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
