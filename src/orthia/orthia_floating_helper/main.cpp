// A test helper for the floating code tests (tests/cli/test_floating.py): it hooks a function of
// its own with the diana patcher, the way hooking code does. Then the hook stub (with the moved
// instructions) and the handler live in executable memory no module owns:
//
//   floating_target_function: jmp <stub>            (the module leads to the stub)
//   stub: ... mov rax, <handler>; call rax; ...      (the stub leads to the handler)
//         <moved instructions>; jmp <target + n>     (and back into the module)
//
// On ARM64 the same with b <stub>, and ldr x16, <handler>; blr x16 in the stub; on A32/Thumb with
// b / ldr pc and ldr r3, <handler>; blx r3.
//
// It also hooks an import: the slot of GetCurrentProcessId in its IAT (getpid in its GOT on Linux)
// leads to a thunk in an allocation of its own, which jumps on to the function. On Windows the
// export table entry of floating_exported_function leads to another thunk.
//
// And a second module leads into the handler allocation: it loads orthia_floating_helper_lib from
// its own folder and writes a jump over floating_lib_function to code at handler + 40h.
//
// Prints "ready target=<hex> stub=<hex> handler=<hex> calls=<n> result=<n> bits=<32|64>
// slot=<hex> thunk=<hex> import=<name> export_slot=<hex> export_thunk=<hex> lib_function=<hex>
// lib_entry=<hex>" (a slot or the lib is 0 when it isn't hooked) and waits until its stdin is
// closed ("change" on stdin changes the handler allocation).

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
#include <dlfcn.h>
#include <elf.h>
#include <link.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#define HELPER_NOINLINE __declspec(noinline)
#else
#define HELPER_NOINLINE __attribute__((noinline))
#endif

// The hooked function starts with plain instructions and a call (call rel32, bl), which the patcher
// moves into its stub when it is among the first instructions (the call is corrected for the new place)
extern "C" HELPER_NOINLINE int floating_leaf_function(int value)
{
    return value + 1;
}
extern "C" HELPER_NOINLINE int floating_target_function(int value)
{
    return floating_leaf_function(value) * 2;
}
#ifdef _WIN32
// gives the executable an export table, whose entry gets hooked
extern "C" __declspec(dllexport) HELPER_NOINLINE int floating_exported_function(int value)
{
    return value + 3;
}
#endif

namespace
{
#if defined(_M_ARM64) || defined(__aarch64__)
    // b reaches 128 MB
    const unsigned long long c_maxDistance = 0x07000000ULL;
#elif defined(_M_ARM) || defined(__arm__)
    // A32 b reaches 32 MB; stay well under it
    const unsigned long long c_maxDistance = 0x01000000ULL;
#else
    const unsigned long long c_maxDistance = 0x70000000ULL;
#endif

    // the CPU runs the code written, not what its instruction cache holds (ARM)
    void FlushCode(void* address, size_t size)
    {
#ifdef _WIN32
        FlushInstructionCache(GetCurrentProcess(), address, size);
#else
        __builtin___clear_cache((char*)address, (char*)address + size);
#endif
    }

    // executable memory near the hint, so that a jmp rel32 (b on ARM64) reaches it
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
        FlushCode((void*)(uintptr_t)offset, (size_t)size);
        *writeBytes = (OPERAND_SIZE)size;
        return DI_SUCCESS;
    }

    // the slot this module calls an imported function through
#ifdef _WIN32
    const char* const g_importName = "GetCurrentProcessId";
    void** FindImportSlot(const char* name)
    {
        auto* base = (unsigned char*)GetModuleHandleW(nullptr);
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress)
            return nullptr;
        for (auto* desc = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); desc->Name; ++desc)
        {
            auto* names = (IMAGE_THUNK_DATA*)(base + (desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk));
            auto* slots = (IMAGE_THUNK_DATA*)(base + desc->FirstThunk);
            for (; names->u1.AddressOfData; ++names, ++slots)
            {
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    continue;
                auto* byName = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
                if (!std::strcmp((const char*)byName->Name, name))
                    return (void**)&slots->u1.Function;
            }
        }
        return nullptr;
    }
    unsigned long long CallImport()
    {
        return GetCurrentProcessId();
    }
#else
    const char* const g_importName = "getpid";
    struct ImportSearch
    {
        const char* name;
        void** slot;
    };
    // the PLT relocations of the program (the first object dl_iterate_phdr reports)
    int SearchProgram(struct dl_phdr_info* info, size_t, void* context)
    {
        auto* search = (ImportSearch*)context;
        const ElfW(Dyn)* dynamic = nullptr;
        for (int i = 0; i < info->dlpi_phnum; ++i)
        {
            if (info->dlpi_phdr[i].p_type == PT_DYNAMIC)
                dynamic = (const ElfW(Dyn)*)(info->dlpi_addr + info->dlpi_phdr[i].p_vaddr);
        }
        if (!dynamic)
            return 1;
        // glibc has made the pointers absolute already; another loader may not
        auto absolute = [info](ElfW(Addr) value) { return value < info->dlpi_addr ? value + info->dlpi_addr : value; };
        ElfW(Addr) jmprel = 0, symtab = 0, strtab = 0;
        size_t size = 0;
        ElfW(Sxword) relType = DT_REL;
        for (const ElfW(Dyn)* d = dynamic; d->d_tag != DT_NULL; ++d)
        {
            switch (d->d_tag)
            {
            case DT_JMPREL: jmprel = absolute(d->d_un.d_ptr); break;
            case DT_PLTRELSZ: size = d->d_un.d_val; break;
            case DT_SYMTAB: symtab = absolute(d->d_un.d_ptr); break;
            case DT_STRTAB: strtab = absolute(d->d_un.d_ptr); break;
            case DT_PLTREL: relType = (ElfW(Sxword))d->d_un.d_val; break;
            }
        }
        if (!jmprel || !symtab || !strtab)
            return 1;
        const size_t entrySize = relType == DT_RELA ? sizeof(ElfW(Rela)) : sizeof(ElfW(Rel));
        for (size_t offset = 0; offset + entrySize <= size; offset += entrySize)
        {
            const auto* rel = (const ElfW(Rel)*)(jmprel + offset);
#if defined(__LP64__)
            const size_t symbol = ELF64_R_SYM(rel->r_info);
#else
            const size_t symbol = ELF32_R_SYM(rel->r_info);
#endif
            const auto* sym = (const ElfW(Sym)*)symtab + symbol;
            if (!std::strcmp((const char*)(strtab + sym->st_name), search->name))
            {
                search->slot = (void**)(info->dlpi_addr + rel->r_offset);
                break;
            }
        }
        return 1;
    }
    void** FindImportSlot(const char* name)
    {
        ImportSearch search = { name, nullptr };
        dl_iterate_phdr(SearchProgram, &search);
        return search.slot;
    }
    unsigned long long CallImport()
    {
        return (unsigned long long)getpid();
    }
#endif

    // jmp <function>, position independent
    size_t MakeThunk(unsigned char* code, unsigned long long function)
    {
#if defined(_M_X64) || defined(__x86_64__)
        const unsigned char jump[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 };   // jmp [rip]
        std::memcpy(code, jump, sizeof(jump));
        std::memcpy(code + sizeof(jump), &function, 8);
        return sizeof(jump) + 8;
#elif defined(_M_IX86) || defined(__i386__)
        const unsigned int value = (unsigned int)function;
        code[0] = 0x68;                                                         // push <function>
        std::memcpy(code + 1, &value, 4);
        code[5] = 0xC3;                                                         // ret
        return 6;
#elif defined(_M_ARM64) || defined(__aarch64__)
        const unsigned char jump[] = { 0x50, 0x00, 0x00, 0x58,                  // ldr x16, #8
                                       0x00, 0x02, 0x1F, 0xD6 };                // br x16
        std::memcpy(code, jump, sizeof(jump));
        std::memcpy(code + sizeof(jump), &function, 8);
        return sizeof(jump) + 8;
#elif defined(_M_ARM)
        // Windows runs Thumb only
        const unsigned char jump[] = { 0xDF, 0xF8, 0x00, 0xF0 };                // ldr.w pc, [pc]
        const unsigned int value = (unsigned int)function;
        std::memcpy(code, jump, sizeof(jump));
        std::memcpy(code + sizeof(jump), &value, 4);
        return sizeof(jump) + 4;
#elif defined(__arm__)
        const unsigned char jump[] = { 0x04, 0xF0, 0x1F, 0xE5 };                // ldr pc, [pc, #-4]
        const unsigned int value = (unsigned int)function;
        std::memcpy(code, jump, sizeof(jump));
        std::memcpy(code + sizeof(jump), &value, 4);
        return sizeof(jump) + 4;
#else
        (void)code;
        (void)function;
        return 0;
#endif
    }

    // written at code: a jump to function through a register loaded with its address, which the
    // analysis takes as a reference to it (a jmp [rip] only reads a pointer). thumb: Thumb code (ARM32)
    size_t MakeAbsoluteJump(unsigned char* code, unsigned long long function, bool thumb)
    {
        (void)thumb;
#if defined(_M_X64) || defined(__x86_64__)
        code[0] = 0x48;                                                         // mov rax, <function>
        code[1] = 0xB8;
        std::memcpy(code + 2, &function, 8);
        code[10] = 0xFF;                                                        // jmp rax
        code[11] = 0xE0;
        return 12;
#elif defined(_M_IX86) || defined(__i386__)
        const unsigned int value = (unsigned int)function;
        code[0] = 0xB8;                                                         // mov eax, <function>
        std::memcpy(code + 1, &value, 4);
        code[5] = 0xFF;                                                         // jmp eax
        code[6] = 0xE0;
        return 7;
#elif defined(_M_ARM64) || defined(__aarch64__)
        return MakeThunk(code, function);                                       // ldr x16, #8; br x16
#elif defined(_M_ARM) || defined(__arm__)
        const unsigned int value = (unsigned int)function;
        if (thumb)
        {
            size_t size = 0;
            if ((uintptr_t)code & 2)
            {
                code[0] = 0x00;                                                 // nop: the literal 4-aligned
                code[1] = 0xBF;
                size = 2;
            }
            const unsigned char jump[] = { 0xDF, 0xF8, 0x00, 0xF0 };            // ldr.w pc, [pc]
            std::memcpy(code + size, jump, sizeof(jump));
            std::memcpy(code + size + sizeof(jump), &value, 4);
            return size + sizeof(jump) + 4;
        }
        const unsigned char jump[] = { 0x04, 0xF0, 0x1F, 0xE5 };                // ldr pc, [pc, #-4]
        std::memcpy(code, jump, sizeof(jump));
        std::memcpy(code + sizeof(jump), &value, 4);
        return sizeof(jump) + 4;
#else
        (void)code;
        (void)function;
        return 0;
#endif
    }

    // what floating_lib_function runs once it jumps into the handler allocation: return value + 5
#if defined(_M_X64) || defined(__x86_64__)
#ifdef _WIN32
    const unsigned char g_libEntry[] = { 0x8D, 0x41, 0x05, 0xC3 };              // lea eax, [rcx+5]; ret
#else
    const unsigned char g_libEntry[] = { 0x8D, 0x47, 0x05, 0xC3 };              // lea eax, [rdi+5]; ret
#endif
#elif defined(_M_IX86) || defined(__i386__)
    const unsigned char g_libEntry[] = { 0x8B, 0x44, 0x24, 0x04,                // mov eax, [esp+4]
                                         0x83, 0xC0, 0x05, 0xC3 };              // add eax, 5; ret
#elif defined(_M_ARM64) || defined(__aarch64__)
    const unsigned char g_libEntry[] = { 0x00, 0x14, 0x00, 0x11,                // add w0, w0, #5
                                         0xC0, 0x03, 0x5F, 0xD6 };              // ret
#elif defined(_M_ARM) || defined(__arm__)
    const unsigned char g_libEntry[] = { 0x40, 0x1D,                            // adds r0, r0, #5 (Thumb)
                                         0x70, 0x47 };                          // bx lr
#else
    const unsigned char g_libEntry[] = { 0 };
#endif
    const size_t c_libEntryOffset = 0x40;

    // floating_lib_function of orthia_floating_helper_lib next to this executable; nullptr when
    // the library isn't there
    void* LoadLibraryFunction()
    {
#ifdef _WIN32
        wchar_t path[MAX_PATH];
        const DWORD size = GetModuleFileNameW(nullptr, path, MAX_PATH);
        if (!size || size >= MAX_PATH)
            return nullptr;
        std::wstring file(path, size);
        file = file.substr(0, file.find_last_of(L"\\/") + 1) + L"orthia_floating_helper_lib.dll";
        HMODULE module = LoadLibraryW(file.c_str());
        return module ? (void*)GetProcAddress(module, "floating_lib_function") : nullptr;
#else
        char path[4096];
        const ssize_t size = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if (size <= 0)
            return nullptr;
        std::string file(path, (size_t)size);
        file = file.substr(0, file.find_last_of('/') + 1) + "orthia_floating_helper_lib.so";
        void* module = dlopen(file.c_str(), RTLD_NOW);
        return module ? dlsym(module, "floating_lib_function") : nullptr;
#endif
    }

    // the library function jumps to code at handler + 40h; false when the library isn't there
    bool LeadLibraryToHandler(unsigned char* handler, unsigned long long& functionAddress, unsigned long long& entryAddress)
    {
        void* function = LoadLibraryFunction();
        if (!function)
            return false;
        unsigned char* entry = handler + c_libEntryOffset;
        std::memcpy(entry, g_libEntry, sizeof(g_libEntry));
        FlushCode(entry, sizeof(g_libEntry));

        bool thumb = false;
        unsigned long long target = (unsigned long long)(uintptr_t)entry;
#if defined(_M_ARM) || defined(__arm__)
        thumb = ((uintptr_t)function & 1) != 0;
        // the entry is Thumb code
        target |= 1;
#endif
        auto* code = (unsigned char*)((uintptr_t)function & ~(uintptr_t)1);
        if (!MakeWritable(code, 32))
            return false;
        const size_t size = MakeAbsoluteJump(code, target, thumb);
        if (!size)
            return false;
        FlushCode(code, size);
        functionAddress = (unsigned long long)(uintptr_t)code;
        entryAddress = (unsigned long long)(uintptr_t)entry;
        return true;
    }

    // the import slot leads to a thunk of its own; false when the slot isn't found
    bool HookImport(unsigned long long target, unsigned long long& slotAddress, unsigned long long& thunkAddress)
    {
        void** slot = FindImportSlot(g_importName);
        if (!slot)
            return false;
        auto* thunk = (unsigned char*)AllocateNear(target, 0x1000);
        if (!thunk)
            return false;
        const size_t size = MakeThunk(thunk, (unsigned long long)(uintptr_t)*slot);
        FlushCode(thunk, size);
        if (!size || !MakeWritable(slot, sizeof(void*)))
            return false;
        unsigned long long entry = (unsigned long long)(uintptr_t)thunk;
#if defined(_M_ARM)
        entry |= 1;
#endif
        *slot = (void*)(uintptr_t)entry;
        slotAddress = (unsigned long long)(uintptr_t)slot;
        thunkAddress = (unsigned long long)(uintptr_t)thunk;
        return true;
    }

#ifdef _WIN32
    // the export table entry of the function leads to a thunk: an RVA, so the thunk must be above
    // the module and within 4 GB of it
    bool HookExport(const char* name, unsigned long long& slotAddress, unsigned long long& thunkAddress)
    {
        auto* base = (unsigned char*)GetModuleHandleW(nullptr);
        auto* dos = (IMAGE_DOS_HEADER*)base;
        auto* nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
        const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (!dir.VirtualAddress)
            return false;
        auto* exports = (IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
        auto* names = (DWORD*)(base + exports->AddressOfNames);
        auto* ordinals = (WORD*)(base + exports->AddressOfNameOrdinals);
        auto* functions = (DWORD*)(base + exports->AddressOfFunctions);
        for (DWORD i = 0; i < exports->NumberOfNames; ++i)
        {
            if (std::strcmp((const char*)(base + names[i]), name))
                continue;
            DWORD* slot = &functions[ordinals[i]];
            const unsigned long long moduleBase = (unsigned long long)(uintptr_t)base;
            auto* thunk = (unsigned char*)AllocateNear(moduleBase + nt->OptionalHeader.SizeOfImage, 0x1000);
            const unsigned long long thunkValue = (unsigned long long)(uintptr_t)thunk;
            if (!thunk || thunkValue < moduleBase || thunkValue - moduleBase > 0xFFFFFFFFULL)
                return false;
            const size_t size = MakeThunk(thunk, moduleBase + *slot);
            FlushCode(thunk, size);
            if (!size || !MakeWritable(slot, sizeof(*slot)))
                return false;
#if defined(_M_ARM)
            // a Thumb thunk
            *slot = (DWORD)(thunkValue - moduleBase) | 1;
#else
            *slot = (DWORD)(thunkValue - moduleBase);
#endif
            slotAddress = (unsigned long long)(uintptr_t)slot;
            thunkAddress = thunkValue;
            return true;
        }
        return false;
    }
#endif

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

    // called by the stub: x64 with the context in rcx, x86 as stdcall(context, esp, registers),
    // ARM64 with the context in x0
#if defined(_M_X64) || defined(__x86_64__)
    const unsigned char g_handler[] = {
        0x48, 0xFF, 0x01,                   // inc qword ptr [rcx]
        0xC3                                // ret
    };
    const int g_mode = DIANA_MODE64;
    const int g_arch = DIANA_ARCH_X86;
#elif defined(_M_IX86) || defined(__i386__)
    const unsigned char g_handler[] = {
        0x8B, 0x44, 0x24, 0x04,             // mov eax, [esp+4]
        0xFF, 0x00,                         // inc dword ptr [eax]
        0xC2, 0x0C, 0x00                    // ret 0Ch
    };
    const int g_mode = DIANA_MODE32;
    const int g_arch = DIANA_ARCH_X86;
#elif defined(_M_ARM64) || defined(__aarch64__)
    const unsigned char g_handler[] = {
        0x09, 0x00, 0x40, 0xF9,             // ldr x9, [x0]
        0x29, 0x05, 0x00, 0x91,             // add x9, x9, #1
        0x09, 0x00, 0x00, 0xF9,             // str x9, [x0]
        0xC0, 0x03, 0x5F, 0xD6              // ret
    };
    const int g_mode = DIANA_MODE64;
    const int g_arch = DIANA_ARCH_AARCH64;
#elif defined(_M_ARM) || defined(__arm__)
    // a Thumb handler (called through blx, so it runs whether the hooked code is A32 or Thumb): (*r0)++
    const unsigned char g_handler[] = {
        0x01, 0x68,                         // ldr r1, [r0]
        0x49, 0x1c,                         // adds r1, r1, #1
        0x01, 0x60,                         // str r1, [r0]
        0x70, 0x47                          // bx lr
    };
    const int g_mode = DIANA_MODE32;
    // DIANA_ARCH_ARM32 or DIANA_ARCH_THUMB, chosen in main from the function pointer's Thumb bit
    const int g_arch = DIANA_ARCH_ARM32;
#define HELPER_ARM32
#else
// TODO: other architectures, once the diana patcher can hook them (docs/orthia.md, Floating code)
#define HELPER_UNSUPPORTED
#endif
}

int main()
{
#ifdef HELPER_UNSUPPORTED
    std::printf("unsupported: no patcher hooks for this architecture\n");
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
    FlushCode(handler, sizeof(g_handler));

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

    int arch = g_arch;
    OPERAND_SIZE hookFunction = (OPERAND_SIZE)(uintptr_t)handler;
#ifdef HELPER_ARM32
    // the function pointer's bit 0 tells A32 from Thumb; the Thumb handler is reached through blx
    arch = (((uintptr_t)&floating_target_function) & 1) ? DIANA_ARCH_THUMB : DIANA_ARCH_ARM32;
    hookFunction |= 1;
#endif

    const int status = DianaHook_PatchStreamEx(&provider, g_mode, arch, target, hookFunction, &options);
    if (status)
    {
        std::printf("error: the patch failed: %d\n", status);
        return 1;
    }

    // through a volatile pointer: the call must go through the hooked bytes
    int (*volatile function)(int) = &floating_target_function;
    const int result = function(1);

    // the import through the thunk gives what it gave before
    const unsigned long long importBefore = CallImport();
    unsigned long long importSlot = 0;
    unsigned long long importThunk = 0;
    if (HookImport(target, importSlot, importThunk) && CallImport() != importBefore)
    {
        std::printf("error: the hooked import gives another value\n");
        return 1;
    }

    unsigned long long exportSlot = 0;
    unsigned long long exportThunk = 0;
#ifdef _WIN32
    if (HookExport("floating_exported_function", exportSlot, exportThunk))
    {
        // GetProcAddress reads the hooked entry: the call goes through the thunk
        auto exported = (int (*)(int))GetProcAddress(GetModuleHandleW(nullptr), "floating_exported_function");
        if (!exported || ((unsigned long long)(uintptr_t)exported & ~1ULL) != exportThunk || exported(1) != 4)
        {
            std::printf("error: the hooked export doesn't lead to its thunk\n");
            return 1;
        }
    }
#endif

    // a second module that leads into the handler allocation; the call goes through the jump
    unsigned long long libFunction = 0;
    unsigned long long libEntry = 0;
    if (LeadLibraryToHandler((unsigned char*)handler, libFunction, libEntry))
    {
        auto viaLibrary = (int (*)(int))LoadLibraryFunction();
        if (viaLibrary(1) != 6)
        {
            std::printf("error: the library function doesn't lead to its code in the handler allocation\n");
            return 1;
        }
    }

    std::printf("ready target=%llx stub=%llx handler=%llx calls=%llu result=%d bits=%d slot=%llx thunk=%llx import=%s "
        "export_slot=%llx export_thunk=%llx lib_function=%llx lib_entry=%llx\n",
        target, g_lastStub, (unsigned long long)(uintptr_t)handler, (unsigned long long)calls, result,
        (int)sizeof(void*) * 8, importSlot, importThunk, g_importName, exportSlot, exportThunk, libFunction, libEntry);
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
