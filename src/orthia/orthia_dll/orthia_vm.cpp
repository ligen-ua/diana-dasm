#include "orthia.h"
#include "orthia_vmlib.h"
#include "orthia_common_reports.h"
#include "orthia_diana_print.h"
#include "orthia_files.h"
#include "orthia_windbg_utils.h"
#include "orthia_vmlib_api_handlers.h"

static void PrintVMUsage()
{
    dprintf("!vm_help                                           Displays this text\n");
    dprintf("!vm_vm_new [--id  <vmid>] <vmname>                 Creates a new VM, should be the hex number\n");
    dprintf("!vm_vm_del <vmid>                                  Deletes the VM specified\n");
    dprintf("!vm_vm_list [--no-header]                          Displays the list of VM's\n");
    dprintf("!vm_vm_info <vmid>                                 Displays the information about VM\n");
    dprintf("!vm_vm_d(b,w,d,q,p) <vmid> <address> <range>       Display the contents of memory in the given range\n");
    dprintf("!vm_vm_call <vmid> <address> [<count>] [--print]   Calls the function inside VM\n");
    dprintf("            [reg=value]                            Specify the registers here like rax=1\n");
    dprintf("!vm_vm_u <vmid> <address> [<range>]                Displays an assembly translation of the specified program code\n");
    dprintf("!vm_vm_def                                         Creates and overwrites the \"Default\" VM with ID == 0\n");
    dprintf("!vm_mod_new <vmid>                                 Creates new module\n");
    dprintf("!vm_mod_del <vmid>  <vm-mod-id>                    Deletes the module\n");
    dprintf("!vm_mod_list <vmid> [--no-header]                  Displays the list of modules\n");
    dprintf("!vm_mod_info <vmid> <vm-mod-id>                    Displays the information about module\n");
    dprintf("!vm_mod_e(b,w,d,q,p) <vmid> <vm-mod-id> <address> <values>   \n                                                   Enters into memory the values that you specify\n");
    dprintf("!vm_mod_write_bin <vmid> <vm-mod-id> <address> <filename>    \n                                                   Enters into memory the contents of the file\n");
    dprintf("!vm_mod_enable <vmid> <vm-mod-id>  <enable-flag>   Enables or disables the module specified\n");
    dprintf("!vm_mod_load <vmid> <vm-mod-id> <address> <filename>    \n                                                   Loads into memory the contents of the PE file\n");
    dprintf("!vm_mod_shcall <vmid> <vm-mod-id> <flag> <function>  [arg1] ... [argN] \n                                                   Calls the shuttle extension: flag 1-no output\n");
}

ORTHIA_DECLARE_API(vm_help)
{
    dprintf("Orthia VM Interface:\n\n");
    PrintVMUsage();
    return S_OK;
}

// vm
ORTHIA_DECLARE_API(vm_vm_new)
{
   ORTHIA_CMD_START


       std::wstring wargs = orthia::UnescapeArg(orthia::ToWideString(orthia::Trim(args)));
       if (wargs.empty())
       {
           throw std::runtime_error("Name expected");
       }
       std::vector<orthia::StringInfo> optionsParts;
       orthia::SplitString(wargs, L" ", &optionsParts);

       std::wstring name;
       int position = -1;
       long long vmID = 0;
       long long * pVmID = 0;
       for(std::vector<orthia::StringInfo>::iterator it = optionsParts.begin(), it_end = optionsParts.end();
            it != it_end;
            ++it)
       {
           if (it->size() == 0)
               continue;

           ++position;
           if (position == 0)
           {
               name = it->ToString();
               continue;
           }
           if (orthia::Trim(it->ToString()) == L"--id")
           {
               ++it;
               if (it == it_end)
               {
                    throw std::runtime_error("Id expected"); 
               }
               pVmID = &vmID;
               orthia::ReadExpressitonValue(orthia::Utf16ToAcp(it->ToString()).c_str(), vmID, true);
               continue;
           }
           throw std::runtime_error("Unknown option"); 
       }

       if (name.empty())
       {
           throw std::runtime_error("Name expected");
       }
       orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
       DoCommand_vm_new(pDatabaseManager, name, pVmID);

   ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_vm_del)
{
   ORTHIA_CMD_START
 
        orthia::Address_type value = 0;
        orthia::ReadExpressitonValue(args, value);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        DoCommand_vm_del(pDatabaseManager, value);

    ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_vm_list)
{
   ORTHIA_CMD_START

        bool useHeader = true;
        std::wstring wargs = orthia::ToWideString(orthia::Trim(args));
        if (wcsncmp(wargs.c_str(), L"--no-header", 11) == 0)
        {
            useHeader = false;
        }
        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::VmInfoListTargetOverVector infoList;
        orthia::DoCommand_vm_list(pDatabaseManager, &infoList);

        orthia::CReport report;
        report.AddColumn(L"Id", 8, orthia::report::ctRight);
        report.AddColumn(L"Creation Time", 24, orthia::report::ctCenter);
        report.AddColumn(L"Modification Time", 24, orthia::report::ctCenter);
        report.AddColumn(L"Name", 20, orthia::report::ctLeft);

        if (useHeader)
        {
            orthia::CReportRow row = report.PrintHeader();
            dprintf("%S\n",  row.GetStr());
        }
        for(orthia::VmInfoListTargetOverVector::const_iterator it = infoList.m_data.begin(), it_end = infoList.m_data.end();
            it != it_end;
            ++it)
        {
            orthia::CReportRow row = report.StartRow()
                << orthia::ToWideStringAsHex_Short(it->id)
                << it->creationTime.GUI_QueryConvertedToLocal()
                << it->lastWriteTime.GUI_QueryConvertedToLocal()
                << it->name
                ;

            dprintf("%S\n",  row.GetStr());
        }

   ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_vm_info)
{
   ORTHIA_CMD_START

        orthia::Address_type value = 0;
        orthia::ReadExpressitonValue(args, value);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::VmInfo info;
        if (!orthia::DoCommand_vm_info(pDatabaseManager, value, &info))
        {
            throw std::runtime_error("VM not found");
        }

        dprintf("ID: %I64lx\nName: %S\nCreation Time: %S\nLast Write Time: %S\n",
                    info.id,
                    info.name.c_str(),
                    info.creationTime.GUI_QueryConvertedToLocal().c_str(),
                    info.lastWriteTime.GUI_QueryConvertedToLocal().c_str());
        
   ORTHIA_CMD_END
}

// mod:common
ORTHIA_DECLARE_API(vm_mod_new)
{
   ORTHIA_CMD_START
 
        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::Address_type vmId = 0;
        orthia::ReadExpressitonValue(args, vmId);

        orthia::DoCommand_vm_mod_new(pDatabaseManager, vmId);

   ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(vm_mod_del)
{
   ORTHIA_CMD_START

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::Address_type vmId = 0, moduleId = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        orthia::ReadExpressitonValue(pTail, moduleId, true);

        orthia::DoCommand_vm_mod_del(pDatabaseManager, vmId, moduleId);

   ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(vm_mod_list)
{
   ORTHIA_CMD_START

        bool useHeader = true;
        std::wstring wargs = orthia::ToWideString(orthia::Trim(args));
        if (wcsncmp(wargs.c_str(), L"--no-header", 11) == 0)
        {
            useHeader = false;
        }

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::Address_type vmId = 0;
        orthia::ReadExpressitonValue(args, vmId);
 
        orthia::GUIVmModuleInfoListTargetOverVector infoList;
        orthia::DoCommand_vm_mod_list(pDatabaseManager, vmId, &infoList);

        const wchar_t * status[] = {L"e", L"d"};
        orthia::CReport report;  
        report.AddColumn(L"Id", 8, orthia::report::ctRight);
        report.AddColumn(L"Lowest Address", 16, orthia::report::ctCenter);
        report.AddColumn(L"Size (bytes)", 16, orthia::report::ctCenter);
        report.AddColumn(L"Writes Count", 16, orthia::report::ctCenter);
        report.AddColumn(L"Status", 6, orthia::report::ctCenter);
        report.AddColumn(L"Description", 30, orthia::report::ctLeft);

        if (useHeader)
        {
            orthia::CReportRow row = report.PrintHeader();
            dprintf("%S\n",  row.GetStr());
        }
        for(orthia::GUIVmModuleInfoListTargetOverVector::const_iterator it = infoList.m_data.begin(), it_end = infoList.m_data.end();
            it != it_end;
            ++it)
        {
            bool isEnabled = !(it->rawInfo.flags & orthia::VmModuleInfo::flags_disabled);
         
            std::wstring description = it->description;
                            
            orthia::CReportRow row = report.StartRow()
                << orthia::ToWideStringAsHex_Short(it->rawInfo.id)
                << orthia::ToWideStringAsHex(it->lowestAddress)
                << orthia::ToWideStringAsHex(it->sizeInBytes)
                << orthia::ToWideStringAsHex_Short(it->writesCount)
                << status[isEnabled?0:1]
                << description
                ;

            dprintf("%S\n",  row.GetStr());
        }

    ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(vm_mod_info)
{
   ORTHIA_CMD_START

        
        orthia::Address_type vmId = 0, moduleId = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        orthia::ReadExpressitonValue(pTail, moduleId, true);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::GUIVmModuleInfo info;
        if (!orthia::DoCommand_vm_mod_info(pDatabaseManager, vmId, moduleId, &info))
        {
            throw std::runtime_error("Module not found");
        }

        const wchar_t * status[] = {L"enabled", L"disabled"};
        dprintf("ID: %I64lx\nLowest Address: %I64lx\nSize (bytes): %I64lx\nDescription: %S\nStatus: %S\n",
                    info.rawInfo.id,
                    info.lowestAddress,
                    info.sizeInBytes,
                    info.description.c_str(),
                    status[info.rawInfo.IsDisabled()?1:0]);

        if (!info.customAttributes.empty())
        {
            dprintf("Additional attributes:\n");

            for(orthia::GUIVmModuleInfo::AttributesMap_type::const_iterator it = info.customAttributes.begin(), it_end = info.customAttributes.end();
                it != it_end;
                ++it)
            {
                dprintf("%S: %S\n", it->first.c_str(), it->second.c_str());
            }
        }
    ORTHIA_CMD_END
}

// mod:special
static HRESULT CALLBACK VmVmRead(PDEBUG_CLIENT4 Client, 
                                 PCSTR args,
                                 char commandMode)
{
   ORTHIA_CMD_START

        bool wow64 = false;
        ULONG machine = DbgExt_GetCurrentModeOfTargetMachine(&wow64);
        int dianaMode = (machine == IMAGE_FILE_MACHINE_AMD64)?8:4;

        int varSize = 0;
        switch(commandMode)
        {
        default:
            throw std::runtime_error("Unknown option");
        case 'b':
            varSize = 1;
            break;
        case 'w':
            varSize = 2;
            break;
        case 'd':
            varSize = 4;
            break;
        case 'q':
            varSize = 8;
            break;
        case 'p':
            varSize = dianaMode;
            break;
        }
        orthia::Address_type vmId = 0, address = 0, sizeInItems = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, address, false);
        
        orthia::ReadWindbgSize(pTail, &sizeInItems);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::CPrintfWriter printfWriter;
        orthia::CVmBinaryMemoryPrinter genericPrinter(&printfWriter,
                                                      varSize, 
                                                      dianaMode,
                                                      -1);

        orthia::Address_type sizeInBytes = sizeInItems*varSize;
        if (sizeInBytes < sizeInItems)
        {
            throw std::runtime_error("Overflow");
        }
        orthia::DoCommand_vm_query_mem(pDatabaseManager, 
                                        vmId, 
                                        true,
                                        address, 
                                        sizeInBytes,
                                        orthia::QueryReader(),
                                        &genericPrinter);
        genericPrinter.Finish();
   ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(vm_vm_db)
{
    return VmVmRead(Client, args, 'b');
}
ORTHIA_DECLARE_API(vm_vm_dw)
{   
    return VmVmRead(Client, args, 'w');
}
ORTHIA_DECLARE_API(vm_vm_dd)
{
    return VmVmRead(Client, args, 'd');
}
ORTHIA_DECLARE_API(vm_vm_dq)
{
    return VmVmRead(Client, args, 'q');
}
ORTHIA_DECLARE_API(vm_vm_dp)
{
    return VmVmRead(Client, args, 'p');
}
ORTHIA_DECLARE_API(vm_vm_u)
{
   ORTHIA_CMD_START

       bool wow64 = false;
       ULONG machine = DbgExt_GetCurrentModeOfTargetMachine(&wow64);
       int dianaMode = (machine == IMAGE_FILE_MACHINE_AMD64)?8:4;

       orthia::Address_type vmId = 0, address = 0, sizeInCommands = 8;
       const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
       pTail = orthia::ReadExpressitonValue(pTail, address, false);
       
       if (!orthia::Trim(pTail).empty())
       {
            orthia::ReadWindbgSize(pTail, &sizeInCommands);
       }

       orthia::Address_type sizeInBytes = sizeInCommands*16;
       if (sizeInBytes < sizeInCommands)
       {
           sizeInBytes = ULLONG_MAX-address;
       }

       orthia::CPrintfWriter printfWriter;
       orthia::CVmAsmMemoryPrinter<diana::CMasmString> asmGenericPrinter(&printfWriter,
                                                                          dianaMode,
                                                                          sizeInCommands);
       orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

       orthia::DoCommand_vm_query_mem(pDatabaseManager, 
                                        vmId, 
                                        true,
                                        address, 
                                        sizeInBytes,
                                        orthia::QueryReader(),
                                        &asmGenericPrinter);

   ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_mod_enable)
{
   ORTHIA_CMD_START
       
        orthia::Address_type vmId = 0, moduleId = 0, enableFlag = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, moduleId, false);
        orthia::ReadExpressitonValue(pTail, enableFlag, true);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::DoCommand_vm_mod_enable(pDatabaseManager, 
                                        vmId, 
                                        moduleId, 
                                        enableFlag != 0);

   ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_mod_write_bin)
{
   ORTHIA_CMD_START

        orthia::Address_type vmId = 0, moduleId = 0, startAddress = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, moduleId, false);
        pTail = orthia::ReadExpressitonValue(pTail, startAddress, false);
        std::wstring filename = orthia::UnescapeArg(orthia::ToWideString(orthia::Trim(pTail)));
        if (filename.empty())
        {
            throw std::runtime_error("Filename expected");
        }

        std::vector<char> buffer;
        orthia::LoadFileToVector(filename, buffer);
        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::DoCommand_vm_mod_write(pDatabaseManager, 
                                       vmId,
                                       moduleId,
                                       startAddress,
                                       buffer);

   ORTHIA_CMD_END
}
static HRESULT CALLBACK VmVmWrite(PDEBUG_CLIENT4 Client, 
                                 PCSTR args,
                                 char commandMode)
{
   ORTHIA_CMD_START

        bool wow64 = false;
        ULONG machine = DbgExt_GetCurrentModeOfTargetMachine(&wow64);
        int dianaMode = (machine == IMAGE_FILE_MACHINE_AMD64)?8:4;

        int varSize = 0;
        switch(commandMode)
        {
        default:
            throw std::runtime_error("Unknown option");
        case 'b':
            varSize = 1;
            break;
        case 'w':
            varSize = 2;
            break;
        case 'd':
            varSize = 4;
            break;
        case 'q':
            varSize = 8;
            break;
        case 'p':
            varSize = dianaMode;
            break;
        }

        orthia::Address_type vmId = 0, moduleId = 0, startAddress = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, moduleId, false);
        pTail = orthia::ReadExpressitonValue(pTail, startAddress, false);

        std::vector<char> buffer;
        orthia::VmDeserializeMemory(varSize, orthia::ToWideString(pTail), &buffer);

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::DoCommand_vm_mod_write(pDatabaseManager, 
                                       vmId,
                                       moduleId,
                                       startAddress,
                                       buffer);
   ORTHIA_CMD_END
}
ORTHIA_DECLARE_API(vm_mod_eb)
{
    return VmVmWrite(Client, args, 'b');
}
ORTHIA_DECLARE_API(vm_mod_ew)
{   
    return VmVmWrite(Client, args, 'w');
}
ORTHIA_DECLARE_API(vm_mod_ed)
{
    return VmVmWrite(Client, args, 'd');
}
ORTHIA_DECLARE_API(vm_mod_eq)
{
    return VmVmWrite(Client, args, 'q');
}
ORTHIA_DECLARE_API(vm_mod_ep)
{
    return VmVmWrite(Client, args, 'p');
}
#define CHECK_REG(RegName) if (orthia::Downcase_Ansi(#RegName) == regValue) { \
    orthia::Address_type value = 0; \
    const char* pTail = orthia::ReadExpressitonValue(orthia::Utf16ToUtf8(optionsParts[1].ToString()).c_str(), value, true); \
    context->reg_##RegName.value = value; \
    return true; \
 }

static bool ApplyRegister(const std::wstring& arg, Diana_Processor_Registers_Context* context) {

    std::vector<orthia::StringInfo> optionsParts;
    std::wstring downcaseArg = orthia::Downcase(arg);
    orthia::SplitString(downcaseArg, L"=", &optionsParts);
    if (optionsParts.size() != 2) {
        return false;
    }
    std::string regValue = orthia::Utf16ToUtf8(optionsParts[0].ToString());
    CHECK_REG(RAX);
    CHECK_REG(RCX);
    CHECK_REG(RDX);
    CHECK_REG(RBX);
    CHECK_REG(RSP);
    CHECK_REG(RBP);
    CHECK_REG(RSI);
    CHECK_REG(RDI);
    CHECK_REG(R8);
    CHECK_REG(R9);
    CHECK_REG(R10);
    CHECK_REG(R11);
    CHECK_REG(R12);
    CHECK_REG(R13);
    CHECK_REG(R14);
    CHECK_REG(R15);
    return false;
}

static
void UnparseCallParameters(const char * pTail, 
                          long long & commandsCount,
                          bool & printData,
                          Diana_Processor_Registers_Context * context)
{
    orthia::Address_type commandsCountRaw = 0;
   
    std::wstring options;
    if (!orthia::ToWideString(orthia::Trim(pTail)).empty())
    {
        pTail = orthia::ReadWindbgSize(pTail, &commandsCountRaw, false, true);
        if (*pTail ==0 || *pTail == L' ')
        {
            commandsCount = (long long)commandsCountRaw;
        }
        options = orthia::UnescapeArg(orthia::ToWideString(orthia::Trim(pTail)));
    }

   
    // parse options
    std::vector<orthia::StringInfo> optionsParts;
    orthia::SplitString(options, L" ", &optionsParts);

    for(std::vector<orthia::StringInfo>::iterator it = optionsParts.begin(), it_end = optionsParts.end();
            it != it_end;
            ++it)
    {
        if (it->size() == 0)
            continue;

        if (orthia::Trim(it->ToString()) == L"--print")
        {
            printData = true;
            continue;
        }
        
        if (ApplyRegister(orthia::Trim(it->ToString()), context))
        {
            continue;
        }

        throw std::runtime_error("Unknown option: " + orthia::Utf16ToUtf8(orthia::Trim(it->ToString())));
    }
}

static 
void PrintCallResults(Diana_Processor_Registers_Context * pContext,
                      int emulationResult,
                      int dianaMode,
                      const std::vector<OPERAND_SIZE> & callStack,
                      orthia::CMemoryStorageOfModifiedData & allWrites,
                      bool printData,
                      long long resCommandsCount)
{
    orthia::PrintResult(pContext, 
                        emulationResult, 
                        dianaMode);

    if (emulationResult && !callStack.empty())
    {
        dprintf("Call Stack\n");
        for(int i = 0; i < (int)callStack.size(); ++i)
        {
            dprintf("    %I64lx\n", callStack[i]);
        }
    }

    dprintf("Commands count: %I64lx\n", resCommandsCount);
    if (printData)
    {
        orthia::PrintData(allWrites, dianaMode);
    }
}


ORTHIA_DECLARE_API(vm_vm_call)
{
   ORTHIA_CMD_START
        
       std::auto_ptr<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
       DbgExt_Context_type contextType = DbgExt_GetDianaContext(context.get());
       if (decNone == contextType)
       {
           throw std::runtime_error("Can't acquire context");
       }
       int dianaMode = contextType == decX64?DIANA_MODE64:DIANA_MODE32;

       orthia::Address_type vmId = 0, startAddress = 0, commandsCountRaw = 0;
       
       const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
       pTail = orthia::ReadExpressitonValue(pTail, startAddress, false);

       bool printData = false;
       long long commandsCount = -1;
       UnparseCallParameters(pTail, 
                             commandsCount,
                             printData,
                             context.get());

       // parse options
       orthia::COrthiaDebugger debugger;
       orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
       orthia::CMemoryStorageOfModifiedData allWrites(0);
       long long resCommandsCount = 0;

       orthia::COrthiaWindbgAPIHandlerDebugInterface debugInterface;
       debugInterface.Reload(dianaMode);
       
       orthia::CWindbgAddressSpace windbgAddressSpace;
       std::vector<OPERAND_SIZE> callStack;
       int emulationResult = orthia::DoCommand_vm_vm_call(pDatabaseManager, 
                                       vmId,
                                       startAddress,
                                       commandsCount,
                                       orthia::QueryReader(),
                                       dianaMode,
                                       context.get(),
                                       &debugger,
                                       allWrites,
                                       &resCommandsCount,
                                       &callStack,
                                       &debugInterface,
                                       &windbgAddressSpace
                                       );

       PrintCallResults(context.get(), 
                        emulationResult,
                        dianaMode,
                        callStack,
                        allWrites,
                        printData,
                        resCommandsCount);


   ORTHIA_CMD_END
}

ORTHIA_DECLARE_API(vm_vm_def)
{
   ORTHIA_CMD_START

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();
        orthia::VmInfo info;
        if (orthia::DoCommand_vm_info(pDatabaseManager, 0, &info))
        {
            orthia::DoCommand_vm_del(pDatabaseManager, 0);
        }
         
        long long id = 0;
        orthia::DoCommand_vm_new(pDatabaseManager, L"Default", &id);
        orthia::DoCommand_vm_mod_new(pDatabaseManager, id);
        
    ORTHIA_CMD_END
}

// vm_mod_load
ORTHIA_DECLARE_API(vm_mod_load)
{
   ORTHIA_CMD_START
       std::auto_ptr<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
       DbgExt_Context_type contextType = DbgExt_GetDianaContext(context.get());
       if (decNone == contextType)
       {
           throw std::runtime_error("Can't acquire context");
       }
       int dianaMode = contextType == decX64?DIANA_MODE64:DIANA_MODE32;

        orthia::Address_type vmId = 0, moduleId = 0, startAddress = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, moduleId, false);
        pTail = orthia::ReadExpressitonValue(pTail, startAddress, false);
        std::wstring filenameArg = orthia::UnescapeArg(orthia::ToWideString(orthia::Trim(pTail)));
        if (filenameArg.empty())
        {
            throw std::runtime_error("Filename expected");
        }

        std::wstring filename = filenameArg;
        if (!orthia::IsFileExists(filename))
        {
            filename = orthia::GetCurrentModuleDir() + filename;
        }

        orthia::COrthiaDebugger debugger;
        std::vector<char> buffer;
        orthia::LoadFileToVector(filename, buffer);
        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::COrthiaWindbgAPIHandlerDebugInterface debugInterface;
        debugInterface.Reload(dianaMode);

        std::vector<OPERAND_SIZE> callStack;
        orthia::COrthiaPeLinkImportsObserver importLoader(&debugInterface);

        orthia::CWindbgAddressSpace windbgAddressSpace;
        long long commandsCount = 0;
        int emulationResult = 
        orthia::DoCommand_vm_mod_load(pDatabaseManager, 
                                       vmId,
                                       moduleId,
                                       startAddress,
                                       buffer,
                                       orthia::QueryReader(),
                                       dianaMode,
                                       &debugger,
                                       &importLoader,
                                       context.get(),
                                       &callStack,
                                       &windbgAddressSpace,
                                       filenameArg,
                                       &debugInterface,
                                       &commandsCount);

        orthia::CMemoryStorageOfModifiedData allWrites(0);
        if (emulationResult && emulationResult != DI_END)
        {
            PrintCallResults(context.get(), 
                            emulationResult,
                            dianaMode,
                            callStack,
                            allWrites,
                            false,
                            commandsCount);
        }


   ORTHIA_CMD_END
}
static
void UnparseShuttleCallParameters(const char * pTail, 
                                  std::vector<orthia::Address_type> * pArguments)
{
    pArguments->clear();

    std::string tail = orthia::Trim(pTail);
    pTail = tail.c_str();

    orthia::Address_type address = 0;
    while(*pTail)
    {
        pTail = orthia::ReadExpressitonValue(pTail, address, false);
        pArguments->push_back(address);
    }
}

ORTHIA_DECLARE_API(vm_mod_shcall)
{
   ORTHIA_CMD_START
       
        std::auto_ptr<Diana_Processor_Registers_Context> context(new Diana_Processor_Registers_Context());
        DbgExt_Context_type contextType = DbgExt_GetDianaContext(context.get());
        if (decNone == contextType)
        {
            throw std::runtime_error("Can't acquire context");
        }
        int dianaMode = contextType == decX64?DIANA_MODE64:DIANA_MODE32;

        std::string function;
        orthia::Address_type vmId = 0, moduleId = 0, startAddress = 0, flag = 0;
        const char * pTail = orthia::ReadExpressitonValue(args, vmId, false);
        pTail = orthia::ReadExpressitonValue(pTail, moduleId, false);
        pTail = orthia::ReadExpressitonValue(pTail, flag, false);
        pTail = orthia::ReadExpressitonValue(pTail, function, false);
       

        bool printData = false;
        long long commandsCount = -1;

        std::vector<orthia::Address_type> arguments;
        UnparseShuttleCallParameters(pTail, 
                                     &arguments);

        orthia::COrthiaDebugger debugger;

        orthia::intrusive_ptr<orthia::CDatabaseManager> pDatabaseManager = orthia::QueryDatabaseManager();

        orthia::COrthiaWindbgAPIHandlerDebugInterface debugInterface;
        debugInterface.Reload(dianaMode);

        std::vector<OPERAND_SIZE> callStack;
        orthia::COrthiaPeLinkImportsObserver importLoader(&debugInterface);

        orthia::CWindbgAddressSpace windbgAddressSpace;

        orthia::CMemoryStorageOfModifiedData allWrites(0);
        long long resCommandsCount = 0;

        bool bSilentMode = (flag & 1) != 0;
        if (!bSilentMode)
        {
            dprintf("Shuttle Output:\n-----  begin -----\n");
        }
        
        int emulationResult = 
        orthia::DoCommand_vm_mod_shcall(pDatabaseManager, 
                                       vmId,
                                       moduleId,
                                       function,
                                       orthia::QueryReader(),
                                       dianaMode,
                                       &debugger,
                                       allWrites,
                                       &importLoader,
                                       context.get(),
                                       &resCommandsCount,
                                       &callStack,
                                       &windbgAddressSpace,
                                       arguments,
                                       &debugInterface);

        if (debugInterface.HaveSomePrintsHappened() || !bSilentMode)
        {
            if (bSilentMode)
            {
                dprintf("\n");
            }
            else
            {
                dprintf("\n-----  end ----- \n");
            }
        }
        
        if (bSilentMode == false || (emulationResult && emulationResult != DI_END))
        {
            PrintCallResults(context.get(), 
                            emulationResult,
                            dianaMode,
                            callStack,
                            allWrites,
                            printData,
                            resCommandsCount);
        }

   ORTHIA_CMD_END
}

// vm_commit
ORTHIA_DECLARE_API(vm_vm_commit)
{
   ORTHIA_CMD_START
       throw std::runtime_error("Not implemented yet");
   ORTHIA_CMD_END
}
