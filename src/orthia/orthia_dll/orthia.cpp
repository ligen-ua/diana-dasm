#include "orthia.h"
#include "orthia_module_manager.h"
#include "orthia_memory_cache.h"
#include "orthia_stack_analyzer.h"
#include "orthia_exec.h"
#include "orthia_windbg_utils.h"
#include "orthia_diana_print.h"

struct ModuleManagerObjects
{
    orthia::CWindbgMemoryReader reader;
    orthia::CModuleManager moduleManager;
    int mode;
    ModuleManagerObjects()
        :
            mode(0)
    {
    }
    orthia::IMemoryReader * GetReader()
    {
        return &reader;
    }
};

static std::auto_ptr<ModuleManagerObjects> g_globals;


namespace orthia
{

static void PrintUsage()
{
    dprintf("!help                                              Displays this text\n");
    dprintf("!profile [/f] <full_name>                          Loads/creates profile (f - force creation)\n");
    dprintf("!lm                                                Displays the list of modules\n");
    dprintf("!reload [/u] [/v] [/f] [/s] <module_address>       Reloads module (u - unload, v - verbose, f - force, s - scan all)\n");
    dprintf("!x [/a] <address>                                  Prints the references which point the address (a - print only addresses)\n");
    dprintf("!xr <address1> <address2>                          Prints the references which point the addresses of the range\n");
    dprintf("!a <address>                                       Analyzes the region\n");
    dprintf("!exec <count>                                      Executes <count> instructions (with current registers)\n");
    dprintf("!u <address1> <address2>                           Prints the code and the references\n");
    dprintf("!vm_help                                           Displays the help for orthia-vm module\n");
}
static void SetupPath(const std::wstring & path, bool bForce)
{
    ULONG machine = DbgExt_GetTargetMachine();
    int mode = 0;
    switch(machine)
    {
    case IMAGE_FILE_MACHINE_I386:
        mode = DIANA_MODE32;
        break;
    case IMAGE_FILE_MACHINE_AMD64:
        mode = DIANA_MODE64;
        break;
    default:
        {
            std::stringstream errorStream;
            errorStream<<"Unsupported target: "<<machine;
            throw std::runtime_error(errorStream.str());
        }
    }

    if (!g_globals.get())
    {
        g_globals.reset(new ModuleManagerObjects);
    }
    g_globals->moduleManager.Reinit(path, bForce);
    g_globals->mode = mode;
    g_globals->reader.Init(mode);
}

static const char * g_pNoProfileMessage = "The current profile is not initialized properly, please run !orthia.profile";
orthia::IMemoryReader * QueryReader()
{
    ModuleManagerObjects * pGlobal = g_globals.get();
    if (!pGlobal) 
    {
        throw std::runtime_error(g_pNoProfileMessage);
    }
    return pGlobal->GetReader();
}
int QueryInitialDianaMode()
{
    ModuleManagerObjects * pGlobal = g_globals.get();
    if (!pGlobal) 
    {
        throw std::runtime_error(g_pNoProfileMessage);
    }
    return pGlobal->mode;
}
orthia::CModuleManager * QueryModuleManager()
{
    ModuleManagerObjects * pGlobal = g_globals.get();
    if (!pGlobal) 
    {
        throw std::runtime_error(g_pNoProfileMessage);
    }
    return &pGlobal->moduleManager;
}

void InitLib()
{
    Diana_Init();
    DianaProcessor_GlobalInit();
    try
    {
        // TODO: add initialization code here

    }
    catch(const std::exception & e)
    {
        &e;
        // do not care;
    }
}


orthia::intrusive_ptr<CDatabaseManager> QueryDatabaseManager()
{
    return QueryModuleManager()->QueryDatabaseManager();
}

} // orthia namespace

// commands
ORTHIA_DECLARE_API(help)
{
    dprintf("Orthia Interface:\n\n");
    orthia::PrintUsage();
    return S_OK;
}

ORTHIA_DECLARE_API(profile)
{
    ORTHIA_CMD_START

        bool bForce = false;
        std::wstring wargs = orthia::ToWideString(orthia::Trim(args));
        if (wcsncmp(wargs.c_str(), L"/f ", 3) == 0)
        {
            bForce = true;
            wargs.erase(0, 3);
            wargs = orthia::Trim(wargs);
        }
        std::wstring path = orthia::ExpandVariable(wargs);
        orthia::SetupPath(path, bForce);

    ORTHIA_CMD_END
}

static std::wstring QueryModuleName(orthia::Address_type address,
                                    std::vector<char> & nameBuffer)
{
    ULONG64 displacement = 0;
    ULONG nameBufferSize = 0;
    nameBuffer.resize(4096);
    DbgExt_GetNameByOffset(address,
                           &nameBuffer.front(),
                           (ULONG)nameBuffer.size(),
                           &nameBufferSize,
                           &displacement);
    
    std::vector<std::string> args;
    args.clear();
    orthia::Split<char>(&nameBuffer.front(), &args, '!');

    std::string moduleName = args.empty()?"<unknown>":args[0];
    return orthia::ToWideString(moduleName);
}
static std::wstring QueryModuleName(orthia::Address_type address)
{
    std::vector<char> nameBuffer;
    return QueryModuleName(address, nameBuffer);
}

ORTHIA_DECLARE_API(lm)
{
    ORTHIA_CMD_START
           
    std::vector<std::string> args;
    std::vector<char> nameBuffer;
    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();
    std::vector<orthia::CommonModuleInfo> modules;
    pModuleManager->QueryLoadedModules(&modules);
    for(std::vector<orthia::CommonModuleInfo>::iterator it = modules.begin(), it_end = modules.end();
        it != it_end;
        ++it)
    {
        
        dprintf("%I64lx     %S\n", it->address, it->name.c_str());
    }
    ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(reload)
{
    ORTHIA_CMD_START
        
    std::vector<std::wstring> words;
    orthia::Split(orthia::ToWideString(args), &words);
    bool bUnload = false;
    orthia::Address_type offset = 0;
    bool offsetInited = false;
    bool bForce = false;
    bool bVerbose = false;
    bool bScanAll = false;
    for(std::vector<std::wstring>::iterator it = words.begin(), it_end = words.end();
        it != it_end;
        ++it)
    {
        if (*it== L"/u")
        {
            bUnload = true;
            continue;
        }
        if (*it== L"/f")
        {
            bForce = true;
            continue;
        }
        if (*it== L"/s")
        {
            bScanAll = true;
            continue;
        }
        if (*it== L"/v")
        {
            bVerbose = true;
            continue;
        }
        if (offsetInited)
            throw std::runtime_error("Unexpected argument: " + orthia::ToAnsiString_Silent(*it));
        PCSTR tail = 0;
        std::string strOffset = orthia::ToAnsiString_Silent(*it);
        GetExpressionEx(strOffset.c_str(), &offset, &tail);
        offsetInited = true;
    }
    
    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();
    if (bUnload)
    {
        pModuleManager->UnloadModule(offset);
        if (bVerbose)
        {
            dprintf("%s %I64lx \n", "Module unloaded: ", offset);
        }
        return S_OK;
    }
    std::wstring name = QueryModuleName(offset);
    int flags = 0;
    if (bScanAll)
    {
        flags |= DI_ANALYSE_PE_FILE_SCAN_THROUGH;
    }
    pModuleManager->ReloadModule(offset, 
                                 orthia::QueryReader(), 
                                 bForce, 
                                 name,
                                 flags);
    if (bVerbose)    
    {        
        dprintf("%s %I64lx  %S\n", "Module loaded: ", offset, name.c_str());
    }
    ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(xr)
{
    ORTHIA_CMD_START

    std::vector<std::string> words;
    orthia::Split<char>(args, &words);

    if (words.size() != 2)
    {
        throw std::runtime_error("Two arguments expected");
    }

    orthia::Address_type address1 = 0, address2 = 0;
    PCSTR tail = 0;
    if (!GetExpressionEx(words[0].c_str(), &address1, &tail))
    {
        throw std::runtime_error("Invalid argument1: " + words[0]);
    }
    if (!GetExpressionEx(words[1].c_str(), &address2, &tail))
    {
        throw std::runtime_error("Invalid argument2: " + words[1]);
    }

    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();

    std::vector<orthia::CommonRangeInfo> references;
    pModuleManager->QueryReferencesToInstructionsRange(address1, address2, &references);

    std::vector<char> nameBuffer;
    for(std::vector<orthia::CommonRangeInfo>::iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        dprintf("%I64lx:\n", it->address);
        
        for(std::vector<orthia::CommonReferenceInfo>::iterator it2 = it->references.begin(), it2_end = it->references.end();
            it2 != it2_end;
            ++it2)
        {
            ULONG64 displacement = 0;
            ULONG nameBufferSize = 0;
            nameBuffer.resize(4096);
            DbgExt_GetNameByOffset(it2->address,
                                   &nameBuffer.front(),
                                   (ULONG)nameBuffer.size(),
                                   &nameBufferSize,
                                   &displacement);

            if (displacement)
            {
                dprintf("    %I64lx     %s+%I64lx\n", it2->address, &nameBuffer.front(), displacement);
            }
            else
            {
                dprintf("    %I64lx     %s\n", it2->address, &nameBuffer.front());
            }
        }
    }   

    ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(x)
{
    ORTHIA_CMD_START

    std::vector<std::wstring> words;
    orthia::Split(orthia::ToWideString(args), &words);

    orthia::Address_type address = 0;
    bool addressInited = false;
    bool bDisplaySymbols = true;
    for(std::vector<std::wstring>::iterator it = words.begin(), it_end = words.end();
        it != it_end;
        ++it)
    {
        if (*it== L"/a")
        {
            bDisplaySymbols = false;
            continue;
        }
        if (addressInited)
            throw std::runtime_error("Unexpected argument: " + orthia::ToAnsiString_Silent(*it));
        PCSTR tail = 0;
        std::string strOffset = orthia::ToAnsiString_Silent(*it);
        if (!GetExpressionEx(strOffset.c_str(), &address, &tail))
        {
            throw std::runtime_error("Address expression expected");
        }
        addressInited = true;
    }

    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();

    std::vector<orthia::CommonReferenceInfo> references;
    pModuleManager->QueryReferencesToInstruction(address, &references);

    std::vector<char> nameBuffer;
    for(std::vector<orthia::CommonReferenceInfo>::iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        if (bDisplaySymbols)
        {
            ULONG64 displacement = 0;
            ULONG nameBufferSize = 0;
            nameBuffer.resize(4096);
            DbgExt_GetNameByOffset(it->address,
                                   &nameBuffer.front(),
                                   (ULONG)nameBuffer.size(),
                                   &nameBufferSize,
                                   &displacement);

            if (displacement)
            {
                dprintf("%I64lx     %s+%I64lx\n", it->address, &nameBuffer.front(), displacement);
            }
            else
            {
                dprintf("%I64lx     %s\n", it->address, &nameBuffer.front());
            }
        }
        else
        {
            dprintf("%I64lx\n", it->address);
        }
    }   
    ORTHIA_CMD_END
}


ORTHIA_DECLARE_API(a)
{
    ORTHIA_CMD_START

    std::vector<std::string> words;
    orthia::Split<char>(args, &words);

    if (words.size() != 1)
    {
        throw std::runtime_error("One argument expected");
    }

    orthia::Address_type address = 0;
    PCSTR tail = 0;
    if (!GetExpressionEx(words[0].c_str(), &address, &tail))
    {
        throw std::runtime_error("Invalid argument: " + words[0]);
    }

    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();
    
    orthia::Address_type size = DbgExt_GetRegionSize(address);
    if (!size)
    {
        throw std::runtime_error("Invalid region: " + words[0]);
    }

    pModuleManager->ReloadRange(address, 
                                size, 
                                orthia::QueryReader(), 
                                g_globals->mode,
                                0);

    ORTHIA_CMD_END
}

// execute text
ORTHIA_DECLARE_API(exec)
{
   ORTHIA_CMD_START

       orthia::Address_type commandsCount = 0;
       PCSTR tail = 0;
       if (!GetExpressionEx(args, &commandsCount, &tail))
       {
           commandsCount = 1;
       }

       std::auto_ptr<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
       DbgExt_Context_type contextType = DbgExt_GetDianaContext(context.get());
       if (decNone == contextType)
       {
           throw std::runtime_error("Can't acquire context");
       }

       orthia::IMemoryReader * pMemoryReader = orthia::QueryReader();
       int dianaMode = contextType == decX64?DIANA_MODE64:DIANA_MODE32;

       orthia::COrthiaDebugger debugger;
       orthia::CMemoryStorageOfModifiedData allWrites(0);
       int emulationResult = orthia::Exec(pMemoryReader, 
                                          dianaMode,
                                          context.get(),
                                          (int)commandsCount,
                                          allWrites,
                                          &debugger,
                                          0,
                                          0);
       orthia::PrintResult(context.get(), 
                           emulationResult, 
                           dianaMode);

       orthia::PrintData(allWrites, dianaMode);

   ORTHIA_CMD_END
}


// scan for stack offset
ORTHIA_DECLARE_API(kv)
{
   ORTHIA_CMD_START

       orthia::Address_type espValue = orthia::QueryRegValue("sp");
       orthia::Address_type eipValue = orthia::QueryRegValue("ip");

       orthia::Address_type levelsCount = 0;
       PCSTR tail = 0;
       if (!GetExpressionEx(args, &levelsCount, &tail))
       {
           levelsCount = 10;
       }
       orthia::IMemoryReader * pMemoryReader = orthia::QueryReader();

       orthia::AnalyzeStack(pMemoryReader,
                             eipValue, 
                             espValue, 
                             levelsCount, 
                             g_globals->mode);
   ORTHIA_CMD_END
}

static
void PrintRange(orthia::CVmAsmMemoryPrinter<diana::CMasmString> * asmGenericPrinter,
                orthia::Address_type address1, 
                orthia::Address_type address2,
                std::vector<char> & cache)
{
    if (address2 < address1)
        throw std::runtime_error("Range error");
    orthia::Address_type size = address2 - address1;
    if (size > 32*1024*1024)
        throw std::runtime_error("Range is too big");

    if (!size)
    {
        return;
    }
    cache.resize((size_t)size);
    orthia::Address_type readBytes = 0;
    orthia::QueryReader()->Read(address1, 
                                size,
                                &cache.front(),
                                &readBytes,
                                0,
                                0,
                                reg_none);
    asmGenericPrinter->OnRange(orthia::VmMemoryRangeInfo(address1, readBytes, orthia::VmMemoryRangeInfo::flags_hasData), 
                               &cache.front());
}

ORTHIA_DECLARE_API(u)
{
    ORTHIA_CMD_START

    int dianaMode = DbgExt_QueryDianaMode();

    std::vector<std::string> words;
    orthia::Split<char>(args, &words);

    if (words.size() != 2)
    {
        throw std::runtime_error("Two arguments expected");
    }

    orthia::Address_type address1 = 0, address2 = 0;
    PCSTR tail = 0;
    if (!GetExpressionEx(words[0].c_str(), &address1, &tail))
    {
        throw std::runtime_error("Invalid argument1: " + words[0]);
    }
    if (!GetExpressionEx(words[1].c_str(), &address2, &tail))
    {
        throw std::runtime_error("Invalid argument2: " + words[1]);
    }

    orthia::CModuleManager * pModuleManager = orthia::QueryModuleManager();

    std::vector<orthia::CommonRangeInfo> references;
    pModuleManager->QueryReferencesToInstructionsRange(address1, address2, &references);

    orthia::CPrintfWriter printfWriter;
    orthia::CVmAsmMemoryPrinter<diana::CMasmString> asmGenericPrinter(&printfWriter,
        dianaMode,
        address2 - address1);

    std::vector<char> nameBuffer;
    std::vector<char> cache;
    orthia::Address_type prevAddress = address1;
    for (std::vector<orthia::CommonRangeInfo>::iterator it = references.begin(), it_end = references.end();
        it != it_end;
        ++it)
    {
        PrintRange(&asmGenericPrinter, prevAddress, it->address, cache);
        prevAddress = it->address;
        dprintf("%I64lx:\n", it->address);

        for (std::vector<orthia::CommonReferenceInfo>::iterator it2 = it->references.begin(), it2_end = it->references.end();
            it2 != it2_end;
            ++it2)
        {
            ULONG64 displacement = 0;
            ULONG nameBufferSize = 0;
            nameBuffer.resize(4096);
            DbgExt_GetNameByOffset(it2->address,
                &nameBuffer.front(),
                (ULONG)nameBuffer.size(),
                &nameBufferSize,
                &displacement);

            if (displacement)
            {
                dprintf("    %I64lx     %s+%I64lx\n", it2->address, &nameBuffer.front(), displacement);
            }
            else
            {
                dprintf("    %I64lx     %s\n", it2->address, &nameBuffer.front());
            }
        }
    }
    PrintRange(&asmGenericPrinter, prevAddress, address2, cache);

    ORTHIA_CMD_END
}
