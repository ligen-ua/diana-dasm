#include "ot_common.h"


static void test_mm1()
{
    OT_TestEnv testEnv;
    
#ifdef WIN32
    void * pFile = GetModuleHandle(0);
#else
    void*  pFile = dlopen(NULL, RTLD_LAZY);
#endif
    
    testEnv.manager.ReloadModule((orthia::Address_type)pFile, &testEnv.reader, true, ORTHIA_TCSTR("test"), 0);

    std::vector<orthia::CommonReferenceInfo> references;
    testEnv.manager.QueryReferencesToInstruction((orthia::Address_type)&test_mm1, &references);
    DIANA_TEST_ASSERT(!references.empty());

    std::vector<orthia::CommonModuleInfo> modules;
    testEnv.manager.QueryLoadedModules(&modules);
    DIANA_TEST_ASSERT(modules.size() == 1);
    DIANA_TEST_ASSERT(modules[0].address == (orthia::Address_type)pFile);

    testEnv.manager.UnloadModule((orthia::Address_type)pFile);
    testEnv.manager.QueryLoadedModules(&modules);
    DIANA_TEST_ASSERT(modules.size() == 0);

    testEnv.manager.QueryReferencesToInstruction((orthia::Address_type)&test_mm1, &references);
    DIANA_TEST_ASSERT(references.empty());
}

#ifdef WIN32
static void test_performance1()
{
    OT_TestEnv testEnv;
    orthia::CDll dll(ORTHIA_TCSTR("shell32.dll"));
    testEnv.manager.ReloadModule((orthia::Address_type)dll.GetBase(), &testEnv.reader, true, ORTHIA_TCSTR("test"), 0);
}
#endif


void test_memory_manager()
{
    DIANA_TEST(test_mm1())
#ifdef WIN32
    DIANA_TEST(test_performance1())
#endif
}