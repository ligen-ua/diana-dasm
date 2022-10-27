extern "C"
{
#include "diana_patchers/diana_ultimate_patcher.h"
#include "diana_streams.h"
}

#include "test_common.h"
#include "string.h"
#include "vector"
#include "test_processor_impl.h"

static int DianaHook_Alloc_test(void * pThis, 
                                 OPERAND_SIZE size, 
                                 OPERAND_SIZE * pAddress,
                                 const OPERAND_SIZE * pHintAddress,
                                 int flags);
static void DianaHook_Free_test(void * pThis, 
                                 const OPERAND_SIZE * pAddress);

class CTestMemoryProvider
{
    CTestMemoryProvider(const CTestMemoryProvider&);
    CTestMemoryProvider & operator = (const CTestMemoryProvider&);
public:
    DianaMemoryStream m_memoryStream;
    DianaHook_Allocator m_allocator;
    DianaHook_TargetMemoryProvider  m_impl;
    std::vector<char> m_heap;

    CTestMemoryProvider(const void * pBuffer,
                        DIANA_SIZE_T size)
    {
        if (size == 0)
            throw std::runtime_error("Invalid size");

        m_heap.assign((char*)pBuffer, (char*)pBuffer+size);
        DianaHook_Allocator_Init(&m_allocator, DianaHook_Alloc_test, DianaHook_Free_test);
        Diana_InitMemoryStreamEx(&m_memoryStream, &m_heap.front(), m_heap.size(), 1);
        DianaHook_TargetMemoryProvider_Init(&m_impl, &m_memoryStream.parent, &m_allocator);
    }
    DianaHook_TargetMemoryProvider  * GetTargetMemoryProvider()
    {
        return &m_impl;
    }
    OPERAND_SIZE Alloc(OPERAND_SIZE size)
    {
        OPERAND_SIZE result = m_heap.size();
        m_heap.resize((size_t)(m_heap.size() + size));
        m_memoryStream.bufferSize = m_heap.size();

        m_memoryStream.pBuffer = &m_heap.front();
        m_memoryStream.bufferSize = m_heap.size();
        return result;
    }

};
int DianaHook_Alloc_test(void * pThis, 
                                 OPERAND_SIZE size, 
                                 OPERAND_SIZE * pAddress,
                                 const OPERAND_SIZE * pHintAddress,
                                 int flags)
{
    CTestMemoryProvider * pProvider = DIANA_CONTAINING_RECORD(pThis, CTestMemoryProvider, m_allocator);
    *pAddress = pProvider->Alloc(size);
    return DI_SUCCESS;
}
void DianaHook_Free_test(void * pThis, 
                                 const OPERAND_SIZE * pAddress)
{
}


static OPERAND_SIZE test_patchers1_impl(unsigned char * pMemory, 
                                size_t sizeOfMemory,
                                OPERAND_SIZE addressToHook,
                                OPERAND_SIZE hookFunction,
                                int processorMode,
                                DianaHook_CustomOptions * pCustomOptions = 0)
{
    CTestMemoryProvider testMemoryProvider(pMemory, 
                                           sizeOfMemory);

    DIANA_TEST_ASSERT(0 == DianaHook_PatchStream(testMemoryProvider.GetTargetMemoryProvider(),
                                           processorMode,
                                           addressToHook,
                                           hookFunction,
                                           pCustomOptions));

    // allocate ESP
    OPERAND_SIZE espEnd = testMemoryProvider.m_heap.size();
    OPERAND_SIZE espStart = ((espEnd&(~0xF))+8)+1024;
    testMemoryProvider.m_heap.resize((size_t)(espStart+8));

    const OPERAND_SIZE eofRIP = 0x12345678;
    *(OPERAND_SIZE *)(&testMemoryProvider.m_heap.front() + espStart) = eofRIP;

    unsigned char * pBuffer = (unsigned char *)&testMemoryProvider.m_heap.front();
    size_t bufferSize = testMemoryProvider.m_heap.size();

    CTestProcessor processor(pBuffer, bufferSize, 0, processorMode);
    DianaProcessor * pCallContext = processor.GetSelf();

    SET_REG_RSP(espStart);
    for(int i = 0; i < 100; ++i)
    {
        OPERAND_SIZE rip = GET_REG_RIP;

        if (rip == eofRIP)
        {
            OPERAND_SIZE rax = GET_REG_RAX;
            return rax;
        }
        OPERAND_SIZE esp = GET_REG_RSP;
        int res = processor.ExecOnce();
        esp = GET_REG_RSP;

        DIANA_TEST_ASSERT(!res);
    }
    return 0;
}

static void test_patchers1_x64(int flags)
{
    DianaHook_CustomOptions options = {0,};
    DianaHook_CustomOptions * pCustomOptions = 0;
    unsigned char data[] = 
    {
        0x40, 0x57,                  // push        rdi  
        0x48, 0x83, 0xEC, 0x40,      // sub         rsp,40h 
        0x48, 0x83, 0xC4, 0x40,      // add         rsp,40h 
        0x5F,                        // pop         rdi  
        0x90,                        // nop
        0x90,                        // nop
        0xC3,                        // ret
        // handler
        0xC3                         // ret
    };
    if (flags)
    {
        options.flags |= flags;
        pCustomOptions = &options;
    }
    test_patchers1_impl(data, 
                        sizeof(data),
                        0,
                        0xE,
                        DIANA_MODE64,
                        pCustomOptions);
}

static void test_patchers1_x64_invalid()
{
    DianaHook_CustomOptions options = {0,};
    unsigned char data[] = 
    {
        0x40, 0x57,                  // push        rdi  
        0x48, 0x83, 0xEC, 0x40,      // sub         rsp,40h 
        0x48, 0x83, 0xC4, 0x40,      // add         rsp,40h 
        0x5F,                        // pop         rdi  
        0xC3,                        // ret
        // handler
        0xC3                         // ret
    };

    options.flags |= DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP;

    CTestMemoryProvider testMemoryProvider(&data, 
                                           sizeof(data));

    DIANA_TEST_ASSERT(DI_END == DianaHook_PatchStream(testMemoryProvider.GetTargetMemoryProvider(),
                                           DIANA_MODE64,
                                           0,
                                           0xC,
                                           &options));
}

static void test_patchers1()
{
    unsigned char data[] = 
    {
        // function to hook
        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0x5D,              // pop         ebp  
        0xC3,              // ret       

        // handler
        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0x5D,              // pop         ebp  
        0xC2, 0x0C, 0x00   // ret   C
    };


    test_patchers1_impl(data, 
                        sizeof(data),
                        0,
                        5,
                        DIANA_MODE32);
}


static void test_patchers1_invalid()
{
    unsigned char data[] = 
    {
        // function to hook
        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0xC3,              // ret       

        // handler
        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0x5D,              // pop         ebp  
        0xC2, 0x0C, 0x00   // ret   C
    };


    CTestMemoryProvider testMemoryProvider(&data, 
                                           sizeof(data));

    DIANA_TEST_ASSERT(DI_END == DianaHook_PatchStream(testMemoryProvider.GetTargetMemoryProvider(),
                                           DIANA_MODE32,
                                           0,
                                           4,
                                           0));

}

static void test_jumps_follow_mode()
{
    unsigned char data[] = 
    {
        // function to hook
        0x90,                         // NOP
        0xE9, 0x0, 0x0, 0x0, 0x0,     // JMP +5

        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0x5D,              // pop         ebp  
        0xC3,              // ret       

        // handler
        0x55,              // push        ebp  
        0x8B, 0xEC,        // mov         ebp,esp 
        0x5D,              // pop         ebp  
        0xC2, 0x0C, 0x00   // ret   C
    };


    test_patchers1_impl(data, 
                        sizeof(data),
                        1,
                        0xB,
                        DIANA_MODE32);
}

static void test_patchers_amd64call()
{
    DianaHook_CustomOptions options = {0,};
    DianaHook_CustomOptions * pCustomOptions = 0;
    unsigned char data[] = 
    {
        0xE8, 0x00, 0x00, 0x00, 0x00,                               // call +5  
        0xC3,                                                       // ret
        // handler
        0x48, 0xb8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 1
        0x49, 0x89, 0x40, 0x18,                                     // mov [r8+18h], rax ; update original RAX
        0xC3                                                        // ret
    };
    OPERAND_SIZE res = test_patchers1_impl(data, 
                        sizeof(data),
                        0,
                        0x6,
                        DIANA_MODE64,
                        pCustomOptions);
    DIANA_TEST_ASSERT(res == 1);
}
void test_patchers()
{
    // i386
    DIANA_TEST(test_patchers1());
    DIANA_TEST(test_jumps_follow_mode());
    DIANA_TEST(test_patchers1_invalid());
    // x64, far jmp
    DIANA_TEST(test_patchers1_x64(DIANA_HOOK_CUSTOM_OPTION_PUT_FAR_JMP));
    // near jmp
    DIANA_TEST(test_patchers1_x64(DIANA_HOOK_CUSTOM_OPTION_PUT_NEAR_JMP));
    // jmp to x<0xFFFFFFFF
    DIANA_TEST(test_patchers1_x64(DIANA_HOOK_CUSTOM_OPTION_TEST_SINGLE_PUSH));
    DIANA_TEST(test_patchers1_x64_invalid());

    // amd64 call
    DIANA_TEST(test_patchers_amd64call());
}

