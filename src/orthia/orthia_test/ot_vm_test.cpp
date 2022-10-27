#include "ot_common.h"
#include "orthia_vmlib.h"
#include "orthia_memory_cache.h"
#include "orthia_common_print.h"
#include "ot_common.h"
#include "orthia_vmlib_api_handlers.h"

extern "C"
{
#include "diana_core_win32_context.h"
#include "diana_processor/diana_processor_win32_context.h"
}
#include "orthia_files.h"


static void test_vm1_1()
{
    CTestDataGenerator testDataGenerator;

    OT_TestEnv testEnv;
    orthia::Ptr<orthia::CDatabaseManager> pDatabaseManager = testEnv.GetDatabaseManager();
    orthia::Ptr<orthia::CVMDatabase> pVmDatabase = pDatabaseManager->GetVMDatabase();
    
    long long vmID1 = pVmDatabase->AddNewVM(L"test");
    DIANA_TEST_ASSERT(vmID1 == 1);

    // check info
    orthia::VmInfo vmInfo;
    DIANA_TEST_ASSERT(pVmDatabase->QueryVMInfo(vmID1, &vmInfo));
    DIANA_TEST_ASSERT(vmInfo.id == 1);
    DIANA_TEST_ASSERT(vmInfo.name == L"test");

    // query all
    orthia::VmInfoListTargetOverVector allVms;
    pVmDatabase->QueryVirtualMachines(&allVms);
    DIANA_TEST_ASSERT(allVms.m_data.size() == 1);
    DIANA_TEST_ASSERT(allVms.m_data[0].id == 1);
    DIANA_TEST_ASSERT(allVms.m_data[0].name == L"test");

    // add second
    DIANA_TEST_EXCEPTION(pVmDatabase->AddNewVM(L"test"), const std::exception & );

    long long vmID2 = pVmDatabase->AddNewVM(L"test2");

    allVms.m_data.clear();
    pVmDatabase->QueryVirtualMachines(&allVms);
    DIANA_TEST_ASSERT(allVms.m_data.size() == 2);
    DIANA_TEST_ASSERT(allVms.m_data[0].id == 1);
    DIANA_TEST_ASSERT(allVms.m_data[0].name == L"test");
    DIANA_TEST_ASSERT(allVms.m_data[1].id == 2);
    DIANA_TEST_ASSERT(allVms.m_data[1].name == L"test2");

    // add empty module
    long long modulePos = pVmDatabase->AddNewModule(vmID2);
    orthia::GUIVmModuleInfoListTargetOverVector moduleList1, moduleList2;
    orthia::DoCommand_vm_mod_list(pDatabaseManager, vmID1, &moduleList1);
    orthia::DoCommand_vm_mod_list(pDatabaseManager, vmID2, &moduleList2);

    DIANA_TEST_ASSERT(moduleList1.m_data.empty());
    DIANA_TEST_ASSERT(moduleList2.m_data.size() == 1);

    DIANA_TEST_ASSERT(moduleList2.m_data[0].rawInfo.id == 1);
    DIANA_TEST_ASSERT(moduleList2.m_data[0].rawInfo.flags == 0);

    orthia::GUIVmModuleInfo module1info;
    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.rawInfo.id == 1);
    
    // let's do some writes
    std::vector<char> buffer1, buffer2, buffer3;

    testDataGenerator.GenerateTestData(buffer2, 1);
    DIANA_TEST_ASSERT(buffer2.size() == 1);
    testDataGenerator.GenerateTestData(buffer3, 1024*1024);
    DIANA_TEST_ASSERT(buffer3.size() == 1024*1024);

    // write 1
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmID2, 
                           1,
                           0x10000000123,
                           buffer1);

    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0);
    DIANA_TEST_ASSERT(module1info.sizeInBytes == 0);
    DIANA_TEST_ASSERT(module1info.writesCount == 0);
    DIANA_TEST_ASSERT(!(module1info.rawInfo.flags & orthia::VmModuleInfo::flags_disabled));

    // write 2
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmID2, 
                           1,
                           0x123ULL,
                           buffer2);


    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0x123ULL);
    DIANA_TEST_ASSERT(module1info.sizeInBytes == buffer2.size());
    DIANA_TEST_ASSERT(module1info.writesCount == 1);

    // write 3
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmID2, 
                           1,
                           0x8000123056700121ULL,
                           buffer3);

    DoCommand_vm_mod_enable(pDatabaseManager,
                            vmID2,
                            1,
                            false);
    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0x123ULL);

    unsigned long long newSize = 0x8000123056700121ULL - 0x123ULL + buffer3.size();
    DIANA_TEST_ASSERT(module1info.sizeInBytes == newSize);
    DIANA_TEST_ASSERT(module1info.writesCount == 2);
    DIANA_TEST_ASSERT(module1info.rawInfo.flags & orthia::VmModuleInfo::flags_disabled);
    
    // write 4
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmID2, 
                           1,
                           0x1024ULL,
                           buffer3);
    DoCommand_vm_mod_enable(pDatabaseManager,
                            vmID2,
                            1,
                            true);

    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0x123ULL);
    DIANA_TEST_ASSERT(module1info.sizeInBytes == newSize);
    DIANA_TEST_ASSERT(module1info.writesCount == 3);
    DIANA_TEST_ASSERT(!(module1info.rawInfo.flags & orthia::VmModuleInfo::flags_disabled));

    // --
    DoCommand_vm_mod_rebuild_metadata(pDatabaseManager, 
                                      vmID2, 
                                      1);

    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            vmID2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0x123ULL);
    DIANA_TEST_ASSERT(module1info.sizeInBytes == newSize);
    DIANA_TEST_ASSERT(module1info.writesCount == 3);
}


static void test_vm1_2()
{
    CTestDataGenerator testDataGenerator;

    orthia::GUIVmModuleInfo module1info;
    OT_TestEnv testEnv(false);
    orthia::Ptr<orthia::CDatabaseManager> pDatabaseManager = testEnv.GetDatabaseManager();
    orthia::Ptr<orthia::CVMDatabase> pVmDatabase = pDatabaseManager->GetVMDatabase();
    
    unsigned long long newSize = 0x8000123056700121ULL - 0x123ULL + 1024*1024;
    DIANA_TEST_ASSERT(DoCommand_vm_mod_info(pDatabaseManager, 
                                            2,
                                            1,
                                            &module1info));
    DIANA_TEST_ASSERT(module1info.lowestAddress == 0x123ULL);
    DIANA_TEST_ASSERT(module1info.sizeInBytes == newSize);
    DIANA_TEST_ASSERT(module1info.writesCount == 3);

    orthia::VmInfoListTargetOverVector allVms;
    pVmDatabase->QueryVirtualMachines(&allVms);
    DIANA_TEST_ASSERT(allVms.m_data.size() == 2);
    //--
    DoCommand_vm_mod_del(pDatabaseManager, 2, 1);
    DIANA_TEST_ASSERT(!DoCommand_vm_mod_info(pDatabaseManager, 
                                            2,
                                            1,
                                            &module1info));
    DoCommand_vm_del(pDatabaseManager, 2);

    allVms.m_data.clear();
    pVmDatabase->QueryVirtualMachines(&allVms);
    DIANA_TEST_ASSERT(allVms.m_data.size() == 1);
}

static void test_vm1()
{
    test_vm1_1();
    test_vm1_2();
}

static void test_vm2()
{
    CTestDataGenerator testDataGenerator;

    OT_TestEnv testEnv;
    orthia::Ptr<orthia::CDatabaseManager> pDatabaseManager = testEnv.GetDatabaseManager();
    orthia::Ptr<orthia::CVMDatabase> pVmDatabase = pDatabaseManager->GetVMDatabase();

    long long vmId1 = pVmDatabase->AddNewVM(L"test");
    long long moduleId1 = pVmDatabase->AddNewModule(vmId1);

    // build our virtual virtual space
    //        [w1:2]        [w4:1] [w3:333]      [w2:2077]
    //         [real-data1]         [data3] [real-data2]
    //         ^                            ^
    //     0x10000                          |
    //                                     0x14000
    //        ************* ********  *** ********
    std::vector<char> initialData1, initialData2, initialData3;
    testDataGenerator.GenerateTestData(initialData1, 0x1000);
    testDataGenerator.GenerateTestData(initialData2, 0x1000);
    testDataGenerator.GenerateTestData(initialData3, 0x100);

    orthia::CReaderOverVector emptyReader(0, 0);
    orthia::CMemoryStorageOfModifiedData realWorld(&emptyReader);
    orthia::Address_type written = 0;
    realWorld.Write(0x10000, 
                    initialData1.size(), 
                    &initialData1.front(),
                    &written,
                    ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                    0,
                    reg_none);

    realWorld.Write(0x14000, 
                    initialData2.size(), 
                    &initialData2.front(),
                    &written,
                    ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                    0,
                    reg_none);

    realWorld.Write(0x12400, 
                    initialData3.size(), 
                    &initialData3.front(),
                    &written,
                    ORTHIA_MR_FLAG_WRITE_ALLOCATE,
                    0,
                    reg_none);

    std::vector<char> write1, write2, write3, write4;
    testDataGenerator.GenerateTestData(write1, 2);
    testDataGenerator.GenerateTestData(write2, 0x2077);
    testDataGenerator.GenerateTestData(write3, 0x333);
    write4.resize(0x1);
    write4[0] = (char)0x99;
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId1, 
                           moduleId1,
                           0x10000-0x1,
                           write1);
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId1, 
                           moduleId1,
                           0x14000+0x1000,
                           write2);
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId1, 
                           moduleId1,
                           0x12345,
                           write3);
    DoCommand_vm_mod_write(pDatabaseManager, 
                           vmId1, 
                           moduleId1,
                           0x11111,
                           write4);

    // test read 1
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x111,
                               0x1000000,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 9);
        DIANA_TEST_ASSERT(ranges.m_data[0].address == 0x111);    // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[1].address == 0x0ff00);  // w1+real1
        DIANA_TEST_ASSERT(ranges.m_data[2].address == 0x11000);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[3].address == 0x11100);  // w4
        DIANA_TEST_ASSERT(ranges.m_data[4].address == 0x11200);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[5].address == 0x12300);  // w3
        DIANA_TEST_ASSERT(ranges.m_data[6].address == 0x12700);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[7].address == 0x14000);  // real2+w2
        DIANA_TEST_ASSERT(ranges.m_data[8].address == 0x17100);  // non-valid

        unsigned long long endOfLastRegion = ranges.m_data[8].address + ranges.m_data[8].size;
        DIANA_TEST_ASSERT(0x1000111ULL == endOfLastRegion);

        // test data
        DIANA_TEST_ASSERT(ranges.m_data[1].data[0x100-1] == write1[0]);
        DIANA_TEST_ASSERT(ranges.m_data[1].data[0x100] == write1[1]);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[1].data[0x101], &initialData1[1], initialData1.size()-1)==0);
        DIANA_TEST_ASSERT(ranges.m_data[3].data[0x11] == write4[0]);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[5].data[0x45], &write3[0], write3.size())==0);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[7].data[0], &initialData2[0], 0x1000)==0);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[7].data[0x1000], &write2[0], write2.size())==0);
    }
    // test read 2
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x10001,
                               0x17100-0x10001-1,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 7);
        DIANA_TEST_ASSERT(ranges.m_data[0].address == 0x10001);  // w1+real1
        DIANA_TEST_ASSERT(ranges.m_data[1].address == 0x11000);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[2].address == 0x11100);  // w4
        DIANA_TEST_ASSERT(ranges.m_data[3].address == 0x11200);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[4].address == 0x12300);  // w3
        DIANA_TEST_ASSERT(ranges.m_data[5].address == 0x12700);  // non-valid
        DIANA_TEST_ASSERT(ranges.m_data[6].address == 0x14000);  // real2+w2
        
        unsigned long long endOfLastRegion = ranges.m_data[6].address + ranges.m_data[6].size;
        DIANA_TEST_ASSERT(0x170FF == endOfLastRegion);

        DIANA_TEST_ASSERT(ranges.m_data[0].data[0] == initialData1[1]);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[6].data[0x1000], &write2[0], write2.size()-1)==0);
    }
 
    // test read 3
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x140FF,
                               0x1234,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 1);
        DIANA_TEST_ASSERT(ranges.m_data[0].address == 0x140FF); 
        DIANA_TEST_ASSERT(ranges.m_data[0].size == 0x1234);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[0].data[0], &initialData2[0xFF], 0x1000-0xFF)==0);
        DIANA_TEST_ASSERT(memcmp(&ranges.m_data[0].data[0x1000-0xFF], &write2[0], 0x1234 - (0x1000-0xFF))==0);
    }

    // test read 4
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x11111,
                               1,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 1);
        DIANA_TEST_ASSERT(ranges.m_data[0].address == 0x11111); 
        DIANA_TEST_ASSERT(ranges.m_data[0].size == 1);
        DIANA_TEST_ASSERT(ranges.m_data[0].data[0] == write4[0]);
    }

    // test read 5
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x11111,
                               0,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 0);
    }

    // test read 6
    {
        orthia::VmMemoryRangesTargetOverVector ranges;
        DoCommand_vm_query_mem(pDatabaseManager, 
                               1,
                               true,
                               0x13FFF,
                               0x2,
                               &realWorld,
                               &ranges);

        DIANA_TEST_ASSERT(ranges.m_data.size() == 2);
        DIANA_TEST_ASSERT(ranges.m_data[0].address == 0x13FFF); 
        DIANA_TEST_ASSERT(ranges.m_data[0].size == 0x1);
        DIANA_TEST_ASSERT(ranges.m_data[1].address == 0x14000); 
        DIANA_TEST_ASSERT(ranges.m_data[1].size == 0x1);
    }
}


void test_vm()
{
    DIANA_TEST(test_vm1())
    DIANA_TEST(test_vm2())
}

class CTestAddressSpace:public orthia::IAddressSpace
{
public:
    virtual bool IsRegionFree(orthia::Address_type offset, 
                              orthia::Address_type size,
                              orthia::Address_type * startOfNewRegion)
    {
        if (startOfNewRegion)
        {
            *startOfNewRegion = 0;
        }
        void * address = (void * )offset;
        MEMORY_BASIC_INFORMATION info;
        memset(&info, 0, sizeof(info));
        if (!VirtualQuery(address, &info, sizeof(info)))
        {
            return false;
        }
        bool isFree = (info.State != MEM_COMMIT);
        if (startOfNewRegion)
        {
            *startOfNewRegion = (orthia::Address_type)info.BaseAddress + info.RegionSize;
        }
        return isFree;
    }
    virtual OPERAND_SIZE GetMaxValidPointer() const
    {
        return MAXSSIZE_T/2;
    }
};

        
static void test_vm_shuttle1()
{
    CTestDataGenerator testDataGenerator;

    OT_TestEnv testEnv;
    orthia::Ptr<orthia::CDatabaseManager> pDatabaseManager = testEnv.GetDatabaseManager();
    orthia::Ptr<orthia::CVMDatabase> pVmDatabase = pDatabaseManager->GetVMDatabase();

    long long vmId1 = pVmDatabase->AddNewVM(L"test_shuttle");
    long long moduleId1 = pVmDatabase->AddNewModule(vmId1);


    int dianaMode = 0;
#ifdef _M_IX86
    DIANA_CONTEXT_NTLIKE_32 context;
    dianaMode = 4;
#else
    DIANA_CONTEXT_NTLIKE_64 context;
    dianaMode = 8;
#endif

    memset(&context, 0, sizeof(context));
    context.ContextFlags = CONTEXT_ALL;
    DIANA_TEST_ASSERT(GetThreadContext(GetCurrentThread(), (LPCONTEXT) &context));

    orthia::CReaderOverRealWorld readerOverCurrentMemory;
    orthia::CMemoryStorageOfModifiedData realWorld(&readerOverCurrentMemory);
    Diana_Processor_Registers_Context dianaContext;
#ifdef _M_IX86
    DI_CHECK_CPP(DianaProcessor_ConvertContextToIndependent_Win32(&context, &dianaContext));
#else
    DI_CHECK_CPP(DianaProcessor_ConvertContextToIndependent_X64(&context, &dianaContext));
#endif

    std::wstring shuttleFileName(L"orthia_shuttle.dll");
    if (GetFileAttributes(shuttleFileName.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        shuttleFileName = orthia::GetCurrentModuleDir() + shuttleFileName;
    }
    std::vector<char> dllData;
    orthia::LoadFileToVector(shuttleFileName.c_str(), dllData);

    CLinkObserverOverWin32 win32Loader;

    HMODULE addressToLoad = LoadLibraryExW(shuttleFileName.c_str(), 0, 0/*LOAD_LIBRARY_AS_IMAGE_RESOURCE*/);
    DIANA_TEST_ASSERT(addressToLoad);
    //FreeLibrary(addressToLoad);

    orthia::CTestDebugger testDebugger;
    CTestAddressSpace testAddressSpace;
    std::vector<OPERAND_SIZE> callStack;
    long long commandsCount = 0;
    int result = 
        orthia::DoCommand_vm_mod_load(pDatabaseManager,
                                  vmId1,
                                  moduleId1,
                                  (DI_UINT64)addressToLoad,
                                  dllData,
                                  &realWorld,
                                  dianaMode,
                                  &testDebugger,
                                  &win32Loader,
                                  &dianaContext,
                                  &callStack,
                                  &testAddressSpace,
                                  shuttleFileName,
                                  &win32Loader,
                                  &commandsCount);


    if (result)
    {
        std::hex(std::cout);
        std::cout<<"Failure stack: \n";
        for(std::vector<OPERAND_SIZE>::const_iterator it = callStack.begin(), it_end = callStack.end();
            it != it_end;
            ++it)
        {
            std::cout<<*it<<"\n";
        }
        std::cout<<"RIP = "<<dianaContext.reg_RIP.value<<"\n";
        std::cout.flush();
    }

    DI_CHECK_CPP(result);
}

void test_vm_shuttle()
{
    DIANA_TEST(test_vm_shuttle1());
}