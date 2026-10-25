// A test helper for the floating code tests (tests/cli/test_floating.py): it hooks a function of
// its own with the diana patcher, the way hooking code does. Then the hook stub (with the moved
// instructions) and the handler live in executable memory no module owns:
//
//   floating_target_function: jmp <stub>            (the module leads to the stub)
//   stub: ... mov rax, <handler>; call rax; ...      (the stub leads to the handler)
//         <moved instructions>; jmp <target + n>     (and back into the module)
//
// Prints "ready target=<hex> stub=<hex> handler=<hex> calls=<n> result=<n> bits=<32|64>" and waits until its
// stdin is closed ("change" on stdin changes the handler allocation). x86 and x64 only: the
// patcher has no ARM support.

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>

extern "C"
{
#include "diana_core.h"
#include "diana_ultimate_patcher.h"
}

#ifdef _WIN32
// the Windows 7 SDK of the XP toolset: objbase.h doesn't build in conformance mode
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#define HELPER_NOINLINE __declspec(noinline)
#else
#define HELPER_NOINLINE __attribute__((noinline))
#endif

// The hooked function starts with plain instructions and a call rel32, which the patcher moves
// into its stub (the call is corrected for the new place)
extern "C" HELPER_NOINLINE int floating_leaf_function(int value)
{
    return value + 1;
}
extern "C" HELPER_NOINLINE int floating_target_function(int value)
{
    return floating_leaf_function(value) * 2;
}

namespace
{
    const unsigned long long c_maxDistance = 0x70000000ULL;

    // executable memory near the hint, so that a jmp rel32 reaches it
    void* AllocateNear(unsigned long long hint, size_t size)
    {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        const unsigned long long granularity = info.dwAllocationGranularity;
        const unsigned long long base = hint & ~(granularity - 1);
        for (unsigned long long delta = granularity; delta < c_maxDistance; delta += granularity)
        {
            const unsigned long long candidates[] = { base + delta, base - delta };
            for (auto candidate : candidates)
            {
                if (candidate < granularity)
                    continue;
                if (void* p = VirtualAlloc((void*)(uintptr_t)candidate, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
                    return p;
            }
        }
        return nullptr;
#else
        const unsigned long long page = (unsigned long long)sysconf(_SC_PAGESIZE);
        const unsigned long long base = hint & ~(page - 1);
        for (unsigned long long delta = 0x100000; delta < c_maxDistance; delta += 0x100000)
        {
            const unsigned long long candidates[] = { base + delta, base - delta };
            for (auto candidate : candidates)
            {
                void* p = mmap((void*)(uintptr_t)candidate, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (p == MAP_FAILED)
                    continue;
                const unsigned long long got = (unsigned long long)(uintptr_t)p;
                const unsigned long long distance = got > hint ? got - hint : hint - got;
                if (distance < c_maxDistance)
                    return p;
                munmap(p, size);
            }
        }
        return nullptr;
#endif
    }

    bool MakeWritable(void* address, size_t size)
    {
#ifdef _WIN32
        DWORD old = 0;
        return VirtualProtect(address, size, PAGE_EXECUTE_READWRITE, &old) != 0;
#else
        const uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
        const uintptr_t start = (uintptr_t)address & ~(page - 1);
        const uintptr_t end = ((uintptr_t)address + size + page - 1) & ~(page - 1);
        return mprotect((void*)start, end - start, PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
#endif
    }

    // This process's own memory as a stream: an offset is an address. Not DianaMemoryStream over
    // the whole address space: it takes (int)(size - offset) as what is left, negative there
    struct SelfMemoryStream
    {
        DianaReadWriteRandomStream parent;
        unsigned long long position = 0;
    };
    int SelfRead(void* pThis, void* pBuffer, int size, int* readBytes)
    {
        auto* stream = (SelfMemoryStream*)pThis;
        std::memcpy(pBuffer, (const void*)(uintptr_t)stream->position, (size_t)size);
        stream->position += (unsigned long long)size;
        *readBytes = size;
        return DI_SUCCESS;
    }
    int SelfMoveTo(void* pThis, OPERAND_SIZE offset)
    {
        ((SelfMemoryStream*)pThis)->position = offset;
        return DI_SUCCESS;
    }
    int SelfRandomRead(void*, OPERAND_SIZE offset, void* pBuffer, int size, OPERAND_SIZE* readBytes, int)
    {
        std::memcpy(pBuffer, (const void*)(uintptr_t)offset, (size_t)size);
        *readBytes = (OPERAND_SIZE)size;
        return DI_SUCCESS;
    }
    int SelfRandomWrite(void*, OPERAND_SIZE offset, void* pBuffer, int size, OPERAND_SIZE* writeBytes, int)
    {
        std::memcpy((void*)(uintptr_t)offset, pBuffer, (size_t)size);
        *writeBytes = (OPERAND_SIZE)size;
        return DI_SUCCESS;
    }

    unsigned long long g_lastStub = 0;

    int Alloc(void*, OPERAND_SIZE size, OPERAND_SIZE* pAddress, const OPERAND_SIZE* pHintAddress, int)
    {
        void* p = AllocateNear(pHintAddress ? *pHintAddress : 0, (size_t)size);
        if (!p)
            return DI_OUT_OF_MEMORY;
        *pAddress = (OPERAND_SIZE)(uintptr_t)p;
        g_lastStub = *pAddress;
        return DI_SUCCESS;
    }
    void Free(void*, const OPERAND_SIZE*)
    {
    }

    // called by the stub: x64 with the context in rcx, x86 as stdcall(context, esp, registers)
#if defined(_M_X64) || defined(__x86_64__)
    const unsigned char g_handler[] = {
        0x48, 0xFF, 0x01,                   // inc qword ptr [rcx]
        0xC3                                // ret
    };
    const int g_mode = DIANA_MODE64;
#elif defined(_M_IX86) || defined(__i386__)
    const unsigned char g_handler[] = {
        0x8B, 0x44, 0x24, 0x04,             // mov eax, [esp+4]
        0xFF, 0x00,                         // inc dword ptr [eax]
        0xC2, 0x0C, 0x00                    // ret 0Ch
    };
    const int g_mode = DIANA_MODE32;
#else
#define HELPER_UNSUPPORTED
#endif
}

int main()
{
#ifdef HELPER_UNSUPPORTED
    std::printf("unsupported: the patcher is x86 and x64 only\n");
    return 2;
#else
    Diana_Init();

    const unsigned long long target = (unsigned long long)(uintptr_t)&floating_target_function;

    // the handler: an allocation of its own
    void* handler = AllocateNear(target, 0x1000);
    if (!handler)
    {
        std::printf("error: no memory for the handler\n");
        return 1;
    }
    std::memcpy(handler, g_handler, sizeof(g_handler));

    if (!MakeWritable((void*)(uintptr_t)target, 64))
    {
        std::printf("error: the function can't be made writable\n");
        return 1;
    }

    SelfMemoryStream stream;
    DianaReadWriteRandomStream_Init(&stream.parent, SelfRead, SelfMoveTo, SelfRandomRead, SelfRandomWrite);
    DianaHook_Allocator allocator;
    DianaHook_Allocator_Init(&allocator, Alloc, Free);
    DianaHook_TargetMemoryProvider provider;
    DianaHook_TargetMemoryProvider_Init(&provider, &stream.parent, &allocator);
    static_assert(offsetof(SelfMemoryStream, parent) == 0, "the stream callbacks get the parent");

    static volatile unsigned long long calls = 0;
    DianaHook_CustomOptions options;
    std::memset(&options, 0, sizeof(options));
    options.flags = DIANA_HOOK_CUSTOM_OPTION_PUT_NEAR_JMP;
    options.patchContext = (OPERAND_SIZE)(uintptr_t)&calls;

    const int status = DianaHook_PatchStream(&provider, g_mode, target, (OPERAND_SIZE)(uintptr_t)handler, &options);
    if (status)
    {
        std::printf("error: the patch failed: %d\n", status);
        return 1;
    }

    // through a volatile pointer: the call must go through the hooked bytes
    int (*volatile function)(int) = &floating_target_function;
    const int result = function(1);

    std::printf("ready target=%llx stub=%llx handler=%llx calls=%llu result=%d bits=%d\n",
        target, g_lastStub, (unsigned long long)(uintptr_t)handler, (unsigned long long)calls, result,
        (int)sizeof(void*) * 8);
    std::fflush(stdout);

    // until the test closes stdin. "change": a byte of the handler allocation changes (past the
    // code), so its floating module is stale on the next open
    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line == "change")
        {
            ((volatile unsigned char*)handler)[0x100] ^= 0xFF;
            std::printf("changed\n");
            std::fflush(stdout);
        }
    }
    return 0;
#endif
}
