#include "test_common.h"
#include "orthia_memory_cache.h"
#include "orthia_files.h"
#include "orthia_streams.h"

extern "C"
{
#include "diana_pe_analyzer.h"
#include "diana_pe.h"
}
#include "Psapi.h"

#include "diana_pe_cpp.h"
#include "ot_common.h"

static void parse_pe_test()
{
    HMODULE hModule = GetModuleHandle(0);
    MODULEINFO moduleInfo = {0, };
    DIANA_TEST_ASSERT(GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo)));

    std::vector<wchar_t> moduleNameBuffer(2048);
    GetModuleFileNameW(hModule, &moduleNameBuffer.front(), (ULONG)moduleNameBuffer.size());
    
    std::vector<char> peFile;
    orthia::LoadFileToVector(&moduleNameBuffer.front(), peFile);
    DIANA_TEST_ASSERT(!peFile.empty());

    DianaMovableReadStreamOverMemory peFileStream;
    DianaMovableReadStreamOverMemory_Init(&peFileStream, &peFile.front(), peFile.size());

    Diana_PeFile dianaPeFile;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile,
                                  &peFileStream.stream,
                                  peFile.size(),
                                  DIANA_PE_FILE_FLAGS_FILE_MODE));
    diana::Guard<diana::PeFile> peFileGuard(&dianaPeFile);

    orthia::CReaderOverVector reader(0, 0);
    orthia::CMemoryStorageOfModifiedData mappedFile(&reader);

    orthia::DianaAnalyzerReadWriteStream writeStream(&mappedFile);
    std::vector<char> page(4096);
    DI_CHECK_CPP(DianaPeFile_Map(&dianaPeFile,
                                 &peFileStream.stream,
                                 (OPERAND_SIZE)hModule,
                                 &writeStream,
                                 &page.front(),
                                 (ULONG)page.size()));

    CLinkObserverOverWin32 win32Loader;
    DI_CHECK_CPP(DianaPeFile_LinkImports(&dianaPeFile,
                                         (OPERAND_SIZE)hModule,
                                         &writeStream,
                                         &page.front(),
                                         (ULONG)page.size(),
                                         win32Loader.GetParent()));
    // compare the loaded modules
    DianaMovableReadStreamOverMemory peFileStream_CurrentModule;
    DianaMovableReadStreamOverMemory_Init(&peFileStream_CurrentModule, hModule, moduleInfo.SizeOfImage);

    Diana_PeFile dianaPeFile_CurrentModule;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile_CurrentModule,
                                  &peFileStream_CurrentModule.stream,
                                  moduleInfo.SizeOfImage,
                                  DIANA_PE_FILE_FLAGS_MODULE_MODE));
    diana::Guard<diana::PeFile> peFileGuard_CurrentModule(&dianaPeFile_CurrentModule);

    // compare headers
    DIANA_TEST_ASSERT(dianaPeFile_CurrentModule.pImpl->sizeOfHeaders);

    std::vector<char> headerBuffer(dianaPeFile_CurrentModule.pImpl->sizeOfHeaders);
    OPERAND_SIZE headerSize = 0;
    DI_CHECK_CPP(writeStream.parent.pRandomRead(&writeStream.parent,
                                                (OPERAND_SIZE)hModule,
                                                &headerBuffer.front(),
                                                (int)headerBuffer.size(),
                                                &headerSize,
                                                0));
    DIANA_TEST_ASSERT(headerSize == headerBuffer.size());

    const char * pHeaderBuffer = &headerBuffer.front();
    if (memcmp(pHeaderBuffer, hModule, headerBuffer.size()))
    {
        const unsigned char * p1 = (const unsigned char * )pHeaderBuffer;
        const unsigned char * p2 = (const unsigned char * )hModule;
        for(OPERAND_SIZE i = 0; i < headerSize; ++i, ++p1, ++p2)
        {
            unsigned char val1 = *p1;
            unsigned char val2 = *p2;
            DIANA_TEST_ASSERT(val1 == val2);
        }
    }

    // grab the real section
    DIANA_IMAGE_SECTION_HEADER * pTextSectionHeader_CurrentModule = DianaPeFile_FindSection(&dianaPeFile_CurrentModule, ".text", 6);
    DIANA_TEST_ASSERT(pTextSectionHeader_CurrentModule);
    const unsigned char * pSectionStart_CurrentModule = (unsigned char*)hModule + pTextSectionHeader_CurrentModule->VirtualAddress;

    // grab the mapped section
    DIANA_IMAGE_SECTION_HEADER * pTextSectionHeader = DianaPeFile_FindSection(&dianaPeFile, ".text", 6);
    DIANA_TEST_ASSERT(pTextSectionHeader);
    DIANA_TEST_ASSERT(pTextSectionHeader->Misc.VirtualSize == pTextSectionHeader_CurrentModule->Misc.VirtualSize);
    DIANA_TEST_ASSERT(pTextSectionHeader->Misc.VirtualSize);

    OPERAND_SIZE sectionStart = (OPERAND_SIZE)hModule + (OPERAND_SIZE)pTextSectionHeader->VirtualAddress;
    std::vector<char> buffer(pTextSectionHeader->Misc.VirtualSize);
    OPERAND_SIZE readBytes = 0;
    DI_CHECK_CPP(writeStream.parent.pRandomRead(&writeStream.parent,
                                                sectionStart,
                                                &buffer.front(),
                                                (int)buffer.size(),
                                                &readBytes,
                                                0));
    DIANA_TEST_ASSERT(readBytes == buffer.size());

    // assume that nobody hooks me, lol
    if (memcmp(&buffer.front(), pSectionStart_CurrentModule, buffer.size()) != 0)
    {
        const unsigned char * pSectionStart = (const unsigned char * )&buffer.front();

        const unsigned char * p1 = pSectionStart;
        const unsigned char * p2 = pSectionStart_CurrentModule;
        for(OPERAND_SIZE i = 0; i < readBytes; ++i, ++p1, ++p2)
        {
            unsigned char val1 = *p1;
            unsigned char val2 = *p2;
            if (val1 != val2 && val2 == 0xCC)
            {
                if (IsDebuggerPresent())
                {
                    continue;
                }
            }
            DIANA_TEST_ASSERT(val1 == val2);
        }
    }
}


static void parse_pe_test2()
{
    HMODULE hNtdllModule = GetModuleHandleW(L"ntdll.dll");
    MODULEINFO ntdllModuleInfo = {0, };
    DIANA_TEST_ASSERT(GetModuleInformation(GetCurrentProcess(), hNtdllModule, &ntdllModuleInfo, sizeof(ntdllModuleInfo)));
    DianaMovableReadStreamOverMemory peFileStream_ntdll;
    DianaMovableReadStreamOverMemory_Init(&peFileStream_ntdll, hNtdllModule, ntdllModuleInfo.SizeOfImage);
    Diana_PeFile dianaPeFile_ntdll;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile_ntdll,
                                  &peFileStream_ntdll.stream,
                                  ntdllModuleInfo.SizeOfImage,
                                  DIANA_PE_FILE_FLAGS_MODULE_MODE));
    diana::Guard<diana::PeFile> peFileGuard_ntdll(&dianaPeFile_ntdll);

    OPERAND_SIZE offset = 0;
    OPERAND_SIZE forwardOffset = 0;
    DI_CHECK_CPP(DianaPeFile_GetProcAddress(&dianaPeFile_ntdll,
                                            (const char * )hNtdllModule,
                                            (const char * )hNtdllModule+ntdllModuleInfo.SizeOfImage,
                                            "RtlCreateHeap",
                                            &offset,
                                            &forwardOffset));

    void * pOriginalAddress = GetProcAddress(hNtdllModule, "RtlCreateHeap");
    const void * pDianaAddress = (const char * )hNtdllModule + offset;
    /// 0x00007ffa276bfe10 NS_FaultTolerantHeap::APIHook_RtlCreateHeap
    // eats my assert DIANA_TEST_ASSERT(pOriginalAddress == pDianaAddress);
    DIANA_TEST_ASSERT(!forwardOffset);

    // test forwards
    HMODULE hkernel32Module = GetModuleHandleW(L"kernel32.dll");
    MODULEINFO kernel32ModuleInfo = {0, };
    DIANA_TEST_ASSERT(GetModuleInformation(GetCurrentProcess(), hkernel32Module, &kernel32ModuleInfo, sizeof(kernel32ModuleInfo)));
    DianaMovableReadStreamOverMemory peFileStream_kernel32;
    DianaMovableReadStreamOverMemory_Init(&peFileStream_kernel32, hkernel32Module, kernel32ModuleInfo.SizeOfImage);
    Diana_PeFile dianaPeFile_kernel32;
    DI_CHECK_CPP(DianaPeFile_Init(&dianaPeFile_kernel32,
                                  &peFileStream_kernel32.stream,
                                  kernel32ModuleInfo.SizeOfImage,
                                  DIANA_PE_FILE_FLAGS_MODULE_MODE));
    diana::Guard<diana::PeFile> peFileGuard_kernel32(&dianaPeFile_kernel32);

    OPERAND_SIZE offset2 = 0;
    OPERAND_SIZE forwardOffset2 = 0;
    DI_CHECK_CPP(DianaPeFile_GetProcAddress(&dianaPeFile_kernel32,
                                            (const char * )hkernel32Module,
                                            (const char * )hkernel32Module+kernel32ModuleInfo.SizeOfImage,
                                            "EnterCriticalSection",
                                            &offset2,
                                            &forwardOffset2));

}
void test_pe()
{
    DIANA_TEST(parse_pe_test());
    DIANA_TEST(parse_pe_test2());
}