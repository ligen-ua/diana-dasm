#include "orthia_vmlib.h"
#include "orthia_vmlib_utils.h"
#include "orthia_exec.h"
#include "diana_core_cpp.h"
#include "orthia_vmlib_api_handlers.h"
extern "C"
{
#include "diana_processor/diana_processor_context.h"
#include "diana_processor/diana_processor_core.h"
#include "diana_pe_analyzer.h"
}
#include "orthia_streams.h"
#include "orthia_vmlib_shuttle.h"

namespace orthia
{

#define ORTHIA_CUSTOM_ATTRIBUTE_PE_ADDRESS_UTF16        L"ca_pe_address"
#define ORTHIA_CUSTOM_ATTRIBUTE_PE_FILENAME_UTF16       L"ca_pe_filename"
#define ORTHIA_CUSTOM_ATTRIBUTE_PE_MODULE_SIZE_UTF16    L"ca_pe_module_size"

long long DoCommand_vm_new(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           const std::wstring & name,
                           const long long * pID)
{
    return pDatabaseManager->GetVMDatabase()->AddNewVM(name, pID);
}

void DoCommand_vm_del(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, long long id)
{
    pDatabaseManager->GetVMDatabase()->DelVM(id);
}

void DoCommand_vm_list(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                         IVmInfoListTarget * pVmListTarget)
{
    pDatabaseManager->GetVMDatabase()->QueryVirtualMachines(pVmListTarget);
}
bool DoCommand_vm_info(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                     long long id,
                     VmInfo * pVmInfo)
{
    return pDatabaseManager->GetVMDatabase()->QueryVMInfo(id, pVmInfo);
}
// modules
long long DoCommand_vm_mod_new(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager,
                               long long vmId)
{
    return pDatabaseManager->GetVMDatabase()->AddNewModule(vmId);
}
void DoCommand_vm_mod_del(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager,
                             long long vmId,
                             long long moduleId)
{
    pDatabaseManager->GetVMDatabase()->DelModule(vmId, moduleId);
}

void DoCommand_vm_mod_list(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                          long long vmId,
                          IGUIVmModuleInfoListTarget * pVmModuleListTarget)
{
    VmModuleInfoListTargetProxy proxy(pVmModuleListTarget, false, true);
    return pDatabaseManager->GetVMDatabase()->QueryModules(vmId, false, &proxy);
}

bool DoCommand_vm_mod_info(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           GUIVmModuleInfo * pVmModuleInfo)
{
    VmModuleInfo info;
    if (!pDatabaseManager->GetVMDatabase()->QueryModuleInfo(vmId, moduleId, false, &info))
    {
        return false;
    }
    CDescriptionManager descriptionManager;
    descriptionManager.Convert(info, true, pVmModuleInfo);
    return true;
}
void DoCommand_vm_mod_write(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           unsigned long long startAddress,
                           const std::vector<char> & buffer)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
    
    CSQLTransaction transaction;
    transaction.Init(pVmDatabase->GetDatabase()->Get());

    GUIVmModuleInfo info;
    if (!pVmDatabase->QueryModuleInfo(vmId, moduleId, true, &info.rawInfo))
    {
        throw std::runtime_error("Module not found");
    }
    if (buffer.empty())
    {
        return;
    }
    CDescriptionManager descriptionManager;
    descriptionManager.Convert(info.rawInfo, false, &info);

    CCommonFormatBuilder builder(descriptionManager.GetParser());
    if (startAddress < info.lowestAddress || info.writesCount == 0)
    {
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS, startAddress);
    }
    if (info.writesCount == 0)
    {
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, (unsigned long long )buffer.size());
    }
    else
    {
        unsigned long long myLastAddress = startAddress + ((unsigned long long )buffer.size() - 1ULL);
        if (myLastAddress < startAddress)
        {
            throw std::runtime_error("Overflow");
        }
        unsigned long long currentLastAddress = info.lowestAddress + (info.sizeInBytes - 1ULL);
        if (myLastAddress > currentLastAddress)
        {
            unsigned long long newSize = info.sizeInBytes + (myLastAddress - currentLastAddress);
            builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, newSize);
        }
    }
    builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT, info.writesCount + 1);
       
    std::string descriptionUtf8;
    builder.Produce(&descriptionUtf8);

    // build new write
    std::vector<char> newWriteData;
    CCommonFormatBuilder writeBuilder;
    writeBuilder.AddMetadata(ORTHIA_WRITE_FIELD_ADDRESS, startAddress);
    writeBuilder.Produce(buffer, &newWriteData);

    // produce
    CCommonFormatMultiBuilder multiBuilder;
    multiBuilder.Init(info.rawInfo.data);
    multiBuilder.AddItem(newWriteData);

    pVmDatabase->UpdateModuleData(vmId,
                                  moduleId,
                                  descriptionUtf8,
                                  multiBuilder.GetData());

    transaction.Commit();
}

void DoCommand_vm_mod_rebuild_metadata(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                                       long long vmId,
                                       long long moduleId)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
    
    CSQLTransaction transaction;
    transaction.Init(pVmDatabase->GetDatabase()->Get());

    GUIVmModuleInfo info;
    if (!pVmDatabase->QueryModuleInfo(vmId, moduleId, true, &info.rawInfo))
    {
        throw std::runtime_error("Module not found");
    }

    CDescriptionManager descriptionManager;
    descriptionManager.Convert(info.rawInfo, false, &info);

    CCommonFormatBuilder builder(descriptionManager.GetParser());

    const unsigned long long moduleStartAddress_initial = 0xFFFFFFFFFFFFFFFFULL;
    const unsigned long long moduleLastAddress_initial = 0;
    unsigned long long moduleStartAddress = moduleStartAddress_initial;
    unsigned long long moduleLastAddress = moduleLastAddress_initial;
    int writesCount = 0;

    // iterate
    CCommonFormatMultiParser multiParser(info.rawInfo.data, false);

    CCommonFormatParser parser;
    while(multiParser.QueryNextItem(&parser))
    {
        unsigned long long currentStart = 0;
        if (!parser.QueryMetadata(ORTHIA_WRITE_FIELD_ADDRESS, &currentStart))
        {
            throw std::runtime_error("Invalid metadata of item, no address");
        }
        unsigned long long currentSize = parser.QuerySizeOfBinary();
        if (currentSize == 0)
        {
            continue;
        }
        unsigned long long currentLast = currentStart + (currentSize-1);
        if (currentLast > moduleLastAddress)
        {
            moduleLastAddress = currentLast;
        }
        if (currentStart < moduleStartAddress)
        {
            moduleStartAddress = currentStart;
        }
        ++writesCount;
    }

    if (writesCount)
    {
        if (moduleLastAddress <= moduleStartAddress)
        {
            throw std::runtime_error("Internal error");
        }
        unsigned long long size = moduleLastAddress - moduleStartAddress + 1;
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT,  writesCount);
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS,  moduleStartAddress);
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, size);
    }
    else
    {
        // no data
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT,  0);
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS,  0);
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, 0);
    }

    std::string descriptionUtf8;
    builder.Produce(&descriptionUtf8);

    pVmDatabase->UpdateModuleXML(vmId,
                                  moduleId,
                                  descriptionUtf8);

    transaction.Commit();
}


void DoCommand_vm_mod_enable(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                             long long vmId,
                             long long moduleId,
                             bool bEnable)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());

    int flagsToSet = VmModuleInfo::flags_disabled, flagsToRemove = 0;
    if (bEnable)
    {
        flagsToSet = 0;
        flagsToRemove = VmModuleInfo::flags_disabled;
    }
    pVmDatabase->UpdateModuleFlags(vmId, 
                                   moduleId, 
                                   flagsToSet,
                                   flagsToRemove);
}

void DoCommand_vm_query_mem(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                            long long vmId,
                            bool onlyEnabled,
                            unsigned long long startAddress,
                            unsigned long long size,
                            IMemoryReader * pReader,
                            IVmMemoryRangesTarget * pTarget)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());

    if (size == 0)
    {
        return;
    }
    if (size > 0x1000000)
    {
        return;
    }
    CVMVirtualSpace virtualSpace(vmId,
                                 onlyEnabled,
                                 startAddress,
                                 size,
                                 pReader);
    virtualSpace.Init(pVmDatabase);

    const CMemoryStorageOfModifiedData & cache = virtualSpace.GetCache();

    VmMemoryRangesTargetGrouppedProxy proxy(pTarget);
    cache.ReportRegions(startAddress,
                               size,
                               &proxy,
                               true);

    proxy.ReportOnce();
}

int DoCommand_vm_vm_call(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                         long long vmId,
                         unsigned long long startAddress,
                         long long commandsCount,
                         IMemoryReader * pReader,
                         int dianaMode,
                         _Diana_Processor_Registers_Context * pContext,
                         IDebugger * pDebugger,
                         orthia::CMemoryStorageOfModifiedData & allWrites,
                         long long * pCommandsCount,
                         std::vector<OPERAND_SIZE> * pCallStack,
                         IAPIHandlerDebugInterface * pAPIHandlerDebugInterface,
                         IAddressSpace * pAddressSpace)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());

    CVMVirtualSpace virtualSpace(vmId,
                                 true,
                                 0,
                                 ULLONG_MAX,
                                 pReader);
    virtualSpace.Init(pVmDatabase);

    CVirtualSpaceClient virtualSpaceClient(pAPIHandlerDebugInterface, &virtualSpace.GetCache());
    orthia::Ptr<IAPIHandler> pAPIHandler = CreateAPIHandler(pAPIHandlerDebugInterface,
                                                            dianaMode,
                                                            pAddressSpace);
    if (pContext->reg_RSP.value == 0)
    {
        throw std::runtime_error("Invalid arguments");
    }
    switch(dianaMode)
    {
    case 4:
    case 8:
        break;
    default:
        throw std::runtime_error("Invalid arguments");
    }
    CMemoryStorageOfModifiedData & cache = virtualSpace.GetCache();
    Address_type originalRSP = pContext->reg_RSP.value;
    Address_type zero = 0;
    Address_type written = 0;
    cache.Write(originalRSP-dianaMode,
                dianaMode,
                &zero,
                &written,
                ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                0,
                reg_none);
    if (written != dianaMode)
    {
        throw std::runtime_error("Invalid arguments");
    }
    pContext->reg_RSP.value -= dianaMode;
    pContext->reg_RIP.value = startAddress;
    return Exec(&cache,
                   dianaMode,
                   pContext,
                   commandsCount,
                   allWrites,
                   pDebugger,
                   pCommandsCount,
                   pAPIHandler,
                   pCallStack);

}


static DI_CHAR g_proxy64[] = 
{
    0x48, 0x83, 0xEC, 0x20,                    // sub         rsp,20h 
    0x49, 0xB8, 0x78, 0x56, 0x34, 0x12, 
                0x78, 0x56, 0x34, 0x12,        // mov         r8,1234567812345678h 
    0xBA, 0x02, 0x00, 0x00, 0x00,              // mov         edx,2 
    0x48, 0xB9, 0x78, 0x56, 0x34, 0x12, 
                0x78, 0x56, 0x34, 0x12,        // mov         rcx,1234567812345678h 
    0xFF, 0x15, 0xF9, 0x6C, 0x32, 0x00,        // call        qword ptr [p (7FF66FA8B008h)] 
    0x48, 0x83, 0xC4, 0x20,                    // add         rsp,20h  
    0xC3                                       // ret             
};

static void GenerateProxy64(DI_UINT64 moduleAddress,
                            DI_UINT32 dwReason,
                            DI_UINT64 lpreserved,
                            DI_UINT64 startAddress,
                            std::vector<char> * pData)
{
    pData->assign(g_proxy64, g_proxy64+sizeof(g_proxy64));
    size_t offsetOfPtr = pData->size();
    if (offsetOfPtr & 0x7)
    {
        offsetOfPtr += 0x8 - (offsetOfPtr & 0x7);
    }
    pData->resize(offsetOfPtr+0x8);
    char * pRawData = &pData->front(); 
    *(DI_UINT64*)(&pRawData[0x6]) = lpreserved;
    *(DI_UINT32*)(&pRawData[0xF]) = dwReason;
    *(DI_UINT64*)(&pRawData[0x15]) = moduleAddress;
    *(DI_UINT32*)(&pRawData[0x1F]) = (DI_UINT32)(offsetOfPtr-0x23);
    *(DI_UINT64*)(&pRawData[offsetOfPtr]) = startAddress;
}


static DI_CHAR g_proxy32[] = 
{
    0x55,                                 // push        ebp  
    0x8B, 0xEC,                           // mov         ebp,esp 
    0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00,   // sub         esp,0C0h 
    0x68, 0x78, 0x56, 0x34, 0x12,         // push        12345678h 
    0x68, 0x78, 0x56, 0x34, 0x12,         // push        12345678h 
    0x68, 0x78, 0x56, 0x34, 0x12,         // push        12345678h
    0xb8, 0x61, 0x00, 0x00, 0xc0,         // mov         eax, 0xc0000061
    0xFF, 0xD0,                           // call        eax
    0x8B, 0xE5,                           // mov         esp,ebp 
    0x5D,                                 // pop         ebp  
    0xC3                                  // ret    
};
static void GenerateProxy32(DI_UINT64 moduleAddress,
                            DI_UINT32 dwReason,
                            DI_UINT64 lpreserved,
                            DI_UINT64 startAddress,
                            std::vector<char> * pData)
{
    pData->assign(g_proxy32, g_proxy32+sizeof(g_proxy32));
    char * pRawData = &pData->front(); 
    *(DI_UINT32*)(&pRawData[0xA]) = (DI_UINT32)lpreserved;
    *(DI_UINT32*)(&pRawData[0xF]) = (DI_UINT32)dwReason;
    *(DI_UINT32*)(&pRawData[0x14]) = (DI_UINT32)moduleAddress;
    *(DI_UINT32*)(&pRawData[0x19]) = (DI_UINT32)startAddress;
}

static
void WriteStub(orthia::CMemoryStorageOfModifiedData & mappedFile, 
               int dianaMode,
               DI_UINT64 arg1, 
               DI_UINT32 arg2, 
               DI_UINT64 arg3,
               DI_UINT64 startAddress,
               DI_UINT64 stubAddress)
{
    std::vector<char> proxy;
    switch(dianaMode)
    {
    case 4:
        GenerateProxy32(arg1, arg2, arg3, startAddress, &proxy);
        break;
    case 8:
        GenerateProxy64(arg1, arg2, arg3, startAddress, &proxy);
        break;
    default:
        throw std::runtime_error("Invalid arguments");
    }

    OPERAND_SIZE written = 0;
    mappedFile.Write(stubAddress, 
                     proxy.size(), 
                     &proxy.front(), 
                     &written,
                     ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                     0,
                     reg_none);

    if (written != proxy.size())
    {
        throw std::runtime_error("Can't write proxy stub");
    }
}


class CVmDatabaseModuleSaver:public IVmMemoryRangesTarget
{
    CDescriptionManager m_descriptionManager;
    GUIVmModuleInfo m_info;
    CCommonFormatMultiBuilder m_multiBuilder;
    std::vector<char> m_newWriteData;
    unsigned long long m_lowestAddress;
    unsigned long long m_highestAddress;
    bool m_wasWrite;

    orthia::intrusive_ptr<CVMDatabase> m_pVmDatabase;
    long long m_vmId;
    long long m_moduleId;
public:
    CVmDatabaseModuleSaver(orthia::intrusive_ptr<CVMDatabase> pVmDatabase,
                           long long vmId,
                            long long moduleId)
        :
            m_lowestAddress(0xFFFFFFFFFFFFFFFFULL),
            m_highestAddress(0),
            m_wasWrite(false),
            m_pVmDatabase(pVmDatabase),
            m_vmId(vmId),
            m_moduleId(moduleId)
    {
        if (!m_pVmDatabase->QueryModuleInfo(vmId, moduleId, true, &m_info.rawInfo))
        {
            throw std::runtime_error("Module not found");
        }
        m_descriptionManager.Convert(m_info.rawInfo, false, &m_info);

        if (m_info.writesCount && m_info.sizeInBytes)
        {
            m_lowestAddress = m_info.lowestAddress;
            m_highestAddress = m_info.lowestAddress + (m_info.sizeInBytes - 1);
            if (m_highestAddress < m_lowestAddress)
            {
                throw std::runtime_error("Overflow");
            }
        }
        m_multiBuilder.Init(m_info.rawInfo.data);
    }
    virtual void OnRange(const VmMemoryRangeInfo & vmRange,
                         const char * pDataStart)
    {
        if (!vmRange.HasData())
        {
            return;
        }
        if (vmRange.size == 0)
        {
            return;
        }
        unsigned long long vmRangeHighestAddress = vmRange.address + (vmRange.size - 1);
        // build new write
        m_newWriteData.clear();
        CCommonFormatBuilder writeBuilder;
        writeBuilder.AddMetadata(ORTHIA_WRITE_FIELD_ADDRESS, vmRange.address);
        writeBuilder.Produce(pDataStart, 
                             pDataStart + vmRange.size,
                             &m_newWriteData);

        m_multiBuilder.AddItem(m_newWriteData);

        if (vmRange.address < m_lowestAddress)
        {
            m_lowestAddress = vmRange.address;
        }
        if (vmRangeHighestAddress > m_highestAddress)
        {
            m_highestAddress = vmRangeHighestAddress;
        }
        m_wasWrite = true;
    }

    void Finalize()
    {
        if (!m_wasWrite)
        {
            return;
        }
        CCommonFormatBuilder builder(m_descriptionManager.GetParser());
        if (m_lowestAddress < m_info.lowestAddress || m_info.writesCount == 0)
        {
            builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_LOWEST_ADDRESS, m_lowestAddress);
        }
        unsigned long long size = m_highestAddress - m_lowestAddress + 1;
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_SIZE_BYTES, size);
        builder.AddMetadata(ORTHIA_DESCRIPTION_FIELD_WRITES_COUNT, m_info.writesCount + 1);
       
        std::string descriptionUtf8;
        builder.Produce(&descriptionUtf8);
        m_pVmDatabase->UpdateModuleData(m_vmId,
                                        m_moduleId,
                                        descriptionUtf8,
                                        m_multiBuilder.GetData());

    }
};
void DoCommand_vm_mod_write(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           CMemoryStorageOfModifiedData & cache,
                           DI_UINT64 startAddress,
                           DI_UINT64 lastAddress)
{
    if (lastAddress == 0)
    {
        lastAddress = 0xFFFFFFFFFFFFFFFFULL;
    }
    if (lastAddress < startAddress)
    {
        throw std::runtime_error("Invalid arguments");
    }
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
    
    CSQLTransaction transaction;
    transaction.Init(pVmDatabase->GetDatabase()->Get());


    CVmDatabaseModuleSaver saver(pVmDatabase, vmId, moduleId);
    unsigned long long size = lastAddress - startAddress + 1;
        
    VmMemoryRangesTargetGrouppedProxy proxy(&saver, 0x100000);
    cache.ReportRegions(startAddress,
                        size,
                        &proxy,
                        false);
    proxy.ReportOnce();
    saver.Finalize();
    transaction.Commit();
}

int DoCommand_vm_mod_load(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           unsigned long long moduleAddress,
                           const std::vector<char> & buffer,

                           IMemoryReader * pReader,
                           int dianaMode,
                           IDebugger * pDebugger,
                           diana::CBasePeLinkImportsObserver * pLinkObserver,
                           _Diana_Processor_Registers_Context * pContext,
                           
                           std::vector<OPERAND_SIZE> * pCallStack,
                           IAddressSpace * pAddressSpace,
                           const std::wstring & fileNameHint,
                           IAPIHandlerDebugInterface * pAPIHandlerDebugInterface,
                           long long * pCommandsCount)
{
    bool kernelModule = false;
    std::wstring fileNameExtension;
    GetExtensionOfFile(fileNameHint, &fileNameExtension);
    if (_wcsicmp(fileNameExtension.c_str(), L"sys")==0)
    {
        kernelModule = true;
    }

    *pCommandsCount = 0;
    const OPERAND_SIZE minValidAddress = 0x2000;
    if (buffer.empty())
    {
        throw std::runtime_error("Invalid arguments");
    }
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
   
    // just check
    GUIVmModuleInfo info;
    if (!pVmDatabase->QueryModuleInfo(vmId, moduleId, true, &info.rawInfo))
    {
        throw std::runtime_error("Module not found");
    }
    if (info.customAttributes.find(ORTHIA_CUSTOM_ATTRIBUTE_PE_ADDRESS_UTF16) != info.customAttributes.end())
    {
        throw std::runtime_error("Can't load the module twice");
    }

    CVMVirtualSpace virtualSpace(vmId,
                                 true,
                                 0,
                                 ULLONG_MAX,
                                 pReader);
    virtualSpace.Init(pVmDatabase);

    CMemoryStorageOfModifiedData & cache = virtualSpace.GetCache();

    CVirtualSpaceClient virtualSpaceClient(pAPIHandlerDebugInterface, &cache);
    orthia::Ptr<IAPIHandler> pAPIHandler = CreateAPIHandler(pAPIHandlerDebugInterface,
                                                            dianaMode,
                                                            pAddressSpace);


    // parse pe file
    DianaMovableReadStreamOverMemory peFileStream;
    DianaMovableReadStreamOverMemory_Init(&peFileStream, 
                                          &buffer.front(), 
                                          buffer.size());

    Diana_PeFile dianaPeFile;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile,
                                  &peFileStream.stream,
                                  buffer.size(),
                                  DIANA_PE_FILE_FLAGS_FILE_MODE));
    diana::Guard<diana::PeFile> peFileGuard(&dianaPeFile);


    if (moduleAddress < minValidAddress)
    {
        if (!cache.ChooseTheRegion(pAddressSpace, 
                                    &moduleAddress,
                                    dianaPeFile.pImpl->sizeOfModule))
        {
            throw std::runtime_error("Can't find the free space for file");
        }
    }


    if (dianaPeFile.pImpl->dianaMode != dianaMode)
    {
        throw std::runtime_error("Unsupported file version");
    }
    // map pe file
    orthia::CMemoryStorageOfModifiedData mappedFile(&cache);
    orthia::CMemoryStorageOfModifiedData allWrites(0);

    orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);
    std::vector<char> page(4096);
    DI_CHECK_CPP(DianaPeFile_Map(&dianaPeFile,
                                 &peFileStream.stream,
                                 moduleAddress,
                                 &writeStream,
                                 &page.front(),
                                 (ULONG)page.size()));

    DI_CHECK_CPP(DianaPeFile_LinkImports(&dianaPeFile,
                                         moduleAddress,
                                         &writeStream,
                                         &page.front(),
                                         (ULONG)page.size(),
                                         pLinkObserver->GetParent()));

    int resultOfDllMain = 0;
    if (dianaPeFile.pImpl->addressOfEntryPoint)
    {
        DI_UINT64 startAddress = moduleAddress + dianaPeFile.pImpl->addressOfEntryPoint;
        Address_type written = 0;
        WriteStub(mappedFile,
                  dianaMode,
                  moduleAddress, 
                  1, 
                  0,
                  startAddress,
                  0x100);


        // simulate call
        Address_type originalRSP = pContext->reg_RSP.value;
        Address_type zero = 0;
        cache.Write(originalRSP-dianaMode,
                    dianaMode,
                    &zero,
                    &written,
                    ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                    0,
                    reg_none);
        if (written != dianaMode)
        {
            throw std::runtime_error("Invalid arguments");
        }
        pContext->reg_RSP.value -= dianaMode;
        pContext->reg_RIP.value = 0x100;

        resultOfDllMain = Exec(&mappedFile,
                               dianaMode,
                               pContext,
                               -1,
                               allWrites,
                               pDebugger,
                               pCommandsCount,
                               pAPIHandler,
                               pCallStack);
        if (resultOfDllMain == DI_END ||
            resultOfDllMain == DI_SUCCESS)
        {
            if (kernelModule)
            {
                resultOfDllMain = pContext->reg_RAX.value == 0?DI_SUCCESS:DI_WIN32_ERROR;
            }
            else
            {
                resultOfDllMain = pContext->reg_RAX.value != 0?DI_SUCCESS:DI_WIN32_ERROR;
            }
        }
        DI_CHECK(resultOfDllMain);
    }
    // save the results
    // commit the mappedFile changes
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId, 
                           moduleId,
                           mappedFile,
                           minValidAddress,
                           0);

    // commit dllmain/driverentry changes
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId, 
                           moduleId,
                           allWrites,
                           minValidAddress,
                           0);

    AttributesToAddMap_type newAttributes;
    newAttributes[ORTHIA_CUSTOM_ATTRIBUTE_PE_ADDRESS_UTF16] = orthia::ToWideStringAsHex_Short(moduleAddress);
    newAttributes[ORTHIA_CUSTOM_ATTRIBUTE_PE_MODULE_SIZE_UTF16] = orthia::ToWideStringAsHex_Short(dianaPeFile.pImpl->sizeOfModule);
    newAttributes[ORTHIA_CUSTOM_ATTRIBUTE_PE_FILENAME_UTF16] = fileNameHint;
    
    DoCommand_vm_mod_manage_custom_attributes(pDatabaseManager, 
                                              vmId, 
                                              moduleId,
                                              AttributesToDeleteSet_type(),
                                              newAttributes);


    return resultOfDllMain;
}


void DoCommand_vm_mod_manage_custom_attributes(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                                               long long vmId,
                                               long long moduleId,
                                               AttributesToDeleteSet_type & attributesToDelete,
                                               AttributesToAddMap_type & attributesToAdd)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
    
    CSQLTransaction transaction;
    transaction.Init(pVmDatabase->GetDatabase()->Get());

    GUIVmModuleInfo info;
    if (!pVmDatabase->QueryModuleInfo(vmId, moduleId, true, &info.rawInfo))
    {
        throw std::runtime_error("Module not found");
    }

    CDescriptionManager descriptionManager;
    descriptionManager.Convert(info.rawInfo, false, &info);

    CCommonFormatBuilder builder(descriptionManager.GetParser());

    // apply
    for(AttributesToDeleteSet_type::const_iterator it = attributesToDelete.begin(), it_end = attributesToDelete.end();
        it != it_end;
        ++it)
    {
        builder.DeleteMetadata(*it);
    }
    for(AttributesToAddMap_type::const_iterator it = attributesToAdd.begin(), it_end = attributesToAdd.end();
        it != it_end;
        ++it)
    {
        builder.AddMetadata(it->first, it->second);
    }

    std::string descriptionUtf8;
    builder.Produce(&descriptionUtf8);
    pVmDatabase->UpdateModuleXML(vmId,
                                 moduleId,
                                 descriptionUtf8);

    transaction.Commit();
}

int DoCommand_vm_mod_shcall(orthia::intrusive_ptr<CDatabaseManager> pDatabaseManager, 
                           long long vmId,
                           long long moduleId,
                           const std::string & function,
                           IMemoryReader * pReader,
                           int dianaMode,
                           IDebugger * pDebugger,
                           orthia::CMemoryStorageOfModifiedData & allWrites,
                           diana::CBasePeLinkImportsObserver * pLinkObserver,
                           _Diana_Processor_Registers_Context * pContext,
                           long long * pCommandsCount,
                           std::vector<OPERAND_SIZE> * pCallStack,
                           IAddressSpace * pAddressSpace,
                           const std::vector<orthia::Address_type> & arguments,
                           IAPIHandlerDebugInterface * pAPIHandlerDebugInterface)
{
    orthia::intrusive_ptr<CVMDatabase> pVmDatabase(pDatabaseManager->GetVMDatabase());
   
    // just check
    GUIVmModuleInfo info;
    if (!DoCommand_vm_mod_info(pDatabaseManager, vmId, moduleId, &info))
    {
        throw std::runtime_error("Module not found");
    }
    GUIVmModuleInfo::AttributesMap_type::const_iterator customAttrIt_address = info.customAttributes.find(ORTHIA_CUSTOM_ATTRIBUTE_PE_ADDRESS_UTF16);
    GUIVmModuleInfo::AttributesMap_type::const_iterator customAttrIt_size = info.customAttributes.find(ORTHIA_CUSTOM_ATTRIBUTE_PE_MODULE_SIZE_UTF16);
    if (customAttrIt_address == info.customAttributes.end() ||
        customAttrIt_size == info.customAttributes.end())
    {
        throw std::runtime_error("Can't find the executable address/size");
    }
    Address_type moduleAddress = orthia::ToAddress(customAttrIt_address->second);
    Address_type moduleSize = orthia::ToAddress(customAttrIt_size->second);
    CVMVirtualSpace virtualSpace(vmId,
                                 true,
                                 0,
                                 ULLONG_MAX,
                                 pReader);
    virtualSpace.Init(pVmDatabase);

    CMemoryStorageOfModifiedData & cache = virtualSpace.GetCache();

    CVirtualSpaceClient virtualSpaceClient(pAPIHandlerDebugInterface, &virtualSpace.GetCache());
    orthia::Ptr<IAPIHandler> pAPIHandler = CreateAPIHandler(pAPIHandlerDebugInterface,
                                                            dianaMode,
                                                            pAddressSpace);

    // acquire the module
    VmMemoryRangesTargetOverVectorPlain module;
    cache.ReportRegions(moduleAddress,
                        moduleSize,
                        &module,
                        true);

    if (module.m_data.empty())
    {
        throw std::runtime_error("Internal error");
    }
    
    // parse PE module 
    DianaMovableReadStreamOverMemory peFileStream;
    DianaMovableReadStreamOverMemory_Init(&peFileStream, 
                                          &module.m_data.front(), 
                                          module.m_data.size());
    Diana_PeFile dianaPeFile;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile,
                                  &peFileStream.stream,
                                  module.m_data.size(),
                                  DIANA_PE_FILE_FLAGS_MODULE_MODE));
    diana::Guard<diana::PeFile> peFileGuard(&dianaPeFile);
    if (dianaPeFile.pImpl->dianaMode != dianaMode)
    {
        throw std::runtime_error("Unsupported file version");
    }

    OPERAND_SIZE offsetOfEntryPoint = 0;
    OPERAND_SIZE forwardOffset = 0;
    DI_CHECK_CPP(DianaPeFile_GetProcAddress(&dianaPeFile,
                                            &module.m_data.front(),
                                            &module.m_data.front() + module.m_data.size(),
                                            function.c_str(),
                                            &offsetOfEntryPoint,
                                            &forwardOffset));
    if (forwardOffset)
    {
        throw std::runtime_error("Can't be forwarded");
    }

    // map pe file
    orthia::CMemoryStorageOfModifiedData mappedFile(&cache);

    DI_UINT64 startAddress = moduleAddress + offsetOfEntryPoint;

    // prepare arguments
    DI_UINT64 argumentsAddress = 0x1000;

    PrebuiltShuttleArgument shuttleArgument;
    CShuttleArgsBuilder shuttleArgsBuilder(argumentsAddress, dianaMode);

    for(std::vector<orthia::Address_type>::const_iterator it = arguments.begin(), it_end = arguments.end();
        it != it_end;
        ++it)
    {
        shuttleArgsBuilder.AddArgument(*it);
    }

    shuttleArgsBuilder.Produce(&shuttleArgument);

    // deploy arguments
    WriteExact(&mappedFile,
               argumentsAddress,
               shuttleArgument.buffer.size(),
               &shuttleArgument.buffer.front(),
               ORTHIA_MR_FLAG_WRITE_ALLOCATE,
               0,
               reg_none);
                 
    WriteStub(mappedFile,
              dianaMode,
              moduleAddress, 
              0, 
              argumentsAddress,
              startAddress,
              0x100);


    // simulate call
    Address_type written = 0;
    Address_type originalRSP = pContext->reg_RSP.value;
    Address_type zero = 0;
    cache.Write(originalRSP-dianaMode,
                dianaMode,
                &zero,
                &written,
                ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                0,
                reg_none);
    if (written != dianaMode)
    {
        throw std::runtime_error("Invalid arguments");
    }
    pContext->reg_RSP.value -= dianaMode;
    pContext->reg_RIP.value = 0x100;

    // register API handlers
    CShuttleAPIHandlerPopulator shuttleAPIHandlerPopulator;
    if (pAPIHandler.get())
    {
        shuttleAPIHandlerPopulator.RegisterHandlers(pAPIHandler.get(), 
                                                    argumentsAddress  + shuttleArgument.trapStartOffset);
    }
    return Exec(&mappedFile,
                dianaMode,
                pContext,
                -1,
                allWrites,
                pDebugger,
                pCommandsCount,
                pAPIHandler,
                pCallStack);
}


}

