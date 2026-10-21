extern "C"
{
#include "diana_streams.h"
#include "diana_arm.h"
#include "diana_executable.h"
#include "diana_text_output_masm.h"
}

#include "test_common.h"
#include "test_analyze_common.h"
#include "string.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "diana_core_cpp.h"
extern "C"
{
#include "diana_elf_analyzer.h"
#include "diana_pe_analyzer.h"
}

// built by tests/arm/make_fixtures.sh from tests/arm/fixtures/*.s
#include "arm_fixtures.inc"

class ArmExecutable
{
    DianaMemoryStream m_stream;
public:
    DianaExecutable executable;

    ArmExecutable(const unsigned char * pData, size_t size)
    {
        Diana_InitMemoryStream(&m_stream, (void*)pData, size);
        DIANA_TEST_ASSERT(DianaExecutable_Init(&executable, &m_stream.parent.parent, size, DIANA_EXECUTABLE_FILE_FLAGS_FILE_MODE) == DI_SUCCESS);
    }
    ~ArmExecutable()
    {
        DianaExecutable_Free(&executable);
    }
    DianaMovableReadStream * GetStream()
    {
        return &m_stream.parent.parent;
    }
};

// decodes one instruction at the file offset through the context the loader asks for
static std::string DisassembleAt(const DianaExecutable & executable,
                                 const unsigned char * pData,
                                 size_t size,
                                 OPERAND_SIZE offset,
                                 OPERAND_SIZE address)
{
    DianaContext context;
    DianaParserResult result;
    DianaMemoryStream stream;
    diana::CMasmString text;
    Diana_InitContextEx(&context, executable.dianaMode, executable.arch);
    Diana_InitMemoryStream(&stream, (void*)(pData + offset), (size_t)(size - offset));
    DIANA_TEST_ASSERT(Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent, &result) == DI_SUCCESS);
    return text.Assign(&result, address);
}

static void test_arm_exe_elf_library()
{
    ArmExecutable file(g_arm64_libfix_so, sizeof(g_arm64_libfix_so));
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_ELF);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE64);

    // exports: the library is linked at 0, file offsets match addresses
    OPERAND_SIZE address = 0;
    int status = DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib_add", &address);
    if (status != DI_SUCCESS)
        std::cout << "DianaElfFile_GetProcAddress: " << status << "\n";
    DIANA_TEST_ASSERT(status == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x220);
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_libfix_so, sizeof(g_arm64_libfix_so), address, address) == "add w0, w0, w1");

    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib_call", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x238);
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_libfix_so, sizeof(g_arm64_libfix_so), address, address) == "stp x29, x30, [sp, #-16]!");
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_libfix_so, sizeof(g_arm64_libfix_so), address + 8, address + 8) == "bl 0x210");
}

// aarch64_be: headers, symbols and data are big-endian, the code is little-endian
static void test_arm_exe_elf_big_endian()
{
    ArmExecutable file(g_arm64be_libfix_so, sizeof(g_arm64be_libfix_so));
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_ELF);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE64);
    DIANA_TEST_ASSERT(file.executable.u.elfFile.pImpl->elfHeader.e_machine == DIANA_EM_AARCH64);

    OPERAND_SIZE address = 0;
    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib_add", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x220);
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64be_libfix_so, sizeof(g_arm64be_libfix_so), address, address) == "add w0, w0, w1");

    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib_call", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x238);
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64be_libfix_so, sizeof(g_arm64be_libfix_so), address + 8, address + 8) == "bl 0x210");
}

// big-endian EM_ARM: BE8 has little-endian code and loads, BE32 has big-endian code and is rejected
static void test_arm_exe_elf_arm_be32()
{
    std::vector<unsigned char> data(g_arm64be_libfix_so, g_arm64be_libfix_so + sizeof(g_arm64be_libfix_so));
    // big-endian e_machine at 0x12, e_flags at 0x30 (ELF64)
    data[0x12] = 0;
    data[0x13] = DIANA_EM_ARM;
    DIANA_TEST_ASSERT(data[0x30] == 0 && data[0x31] == 0 && data[0x32] == 0 && data[0x33] == 0);

    DianaMemoryStream stream;
    DianaExecutable executable;
    Diana_InitMemoryStream(&stream, &data[0], data.size());
    DIANA_TEST_ASSERT(DianaElfFile_Init(&executable.u.elfFile, &stream.parent.parent, data.size(), DIANA_EXECUTABLE_FILE_FLAGS_FILE_MODE) == DI_UNSUPPORTED);

    data[0x31] = (unsigned char)(DIANA_EF_ARM_BE8 >> 16);
    Diana_InitMemoryStream(&stream, &data[0], data.size());
    DIANA_TEST_ASSERT(DianaExecutable_Init(&executable, &stream.parent.parent, data.size(), DIANA_EXECUTABLE_FILE_FLAGS_FILE_MODE) == DI_SUCCESS);
    DIANA_TEST_ASSERT(executable.arch == DIANA_ARCH_ARM32);
    DianaExecutable_Free(&executable);
}

// AArch32 (M6): decodes the instructions at offset with one context, so the T32 IT state goes on
static std::vector<std::string> DisassembleArm32(const unsigned char * pData, size_t size, OPERAND_SIZE offset, int arch, int count)
{
    DianaContext context;
    std::vector<std::string> result;
    Diana_InitContextEx(&context, DIANA_MODE32, arch);
    for (int i = 0; i < count; ++i)
    {
        DianaParserResult parsed;
        DianaMemoryStream stream;
        diana::CMasmString text;
        Diana_InitMemoryStream(&stream, (void*)(pData + offset), (size_t)(size - offset));
        DIANA_TEST_ASSERT(Diana_ParseCmd(&context, Diana_GetRootLine(), &stream.parent.parent.parent, &parsed) == DI_SUCCESS);
        result.push_back(text.Assign(&parsed, offset));
        offset += parsed.iFullCmdSize;
    }
    return result;
}

static void test_arm_exe_elf_arm32()
{
    const unsigned char * pData = g_arm32_libfix_so;
    const size_t size = sizeof(g_arm32_libfix_so);
    ArmExecutable file(pData, size);
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_ELF);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_ARM32);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE32);
    DIANA_TEST_ASSERT(file.executable.u.elfFile.pImpl->elfHeader.e_machine == DIANA_EM_ARM);

    // A32 exports
    OPERAND_SIZE address = 0;
    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib32_add", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x198);
    std::vector<std::string> lines = DisassembleArm32(pData, size, address, DIANA_ARCH_ARM32, 2);
    DIANA_TEST_ASSERT(lines[0] == "add r0, r0, r1" && lines[1] == "bx lr");
    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib32_literal", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(DisassembleArm32(pData, size, address, DIANA_ARCH_ARM32, 1)[0] == "ldr r0, [pc] @ 0x1a8");

    // a T32 function symbol has bit 0 set; the IT block goes on from one instruction to the next
    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib32_thumb", &address) == DI_SUCCESS);
    DIANA_TEST_ASSERT(address == 0x1ad);
    lines = DisassembleArm32(pData, size, address & ~1, DIANA_ARCH_THUMB, 6);
    DIANA_TEST_ASSERT(lines[0] == "cmp r0, #0" && lines[1] == "it eq" && lines[2] == "moveq r0, #1");
    DIANA_TEST_ASSERT(lines[3] == "push {r4, lr}" && lines[4] == "blx 0x18c" && lines[5] == "pop {r4, pc}");
    // VFP and NEON in T32
    DIANA_TEST_ASSERT(DianaElfFile_GetProcAddress(&file.executable.u.elfFile, file.GetStream(), "lib32_fp", &address) == DI_SUCCESS);
    lines = DisassembleArm32(pData, size, address & ~1, DIANA_ARCH_THUMB, 4);
    DIANA_TEST_ASSERT(lines[0] == "vadd.f32 s0, s0, s1" && lines[1] == "vld1.32 {d0-d1}, [r0]!");
    DIANA_TEST_ASSERT(lines[2] == "vmla.f32 q0, q1, q2" && lines[3] == "bx lr");
}

// Windows on ARM 32-bit (ARMNT): PE32, the code is T32
static void test_arm_exe_pe_armnt()
{
    const unsigned char * pData = g_armnt_pe_dll;
    const size_t size = sizeof(g_armnt_pe_dll);
    ArmExecutable file(pData, size);
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_PE);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_THUMB);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE32);
    // .text at file offset 0x200
    std::vector<std::string> lines = DisassembleArm32(pData, size, 0x200, file.executable.arch, 7);
    DIANA_TEST_ASSERT(lines[0] == "adds r0, r0, r1" && lines[1] == "bx lr" && lines[2] == "movs r0, #42 @ 0x2a");
    DIANA_TEST_ASSERT(lines[4] == "it eq" && lines[5] == "moveq r0, #1");
    DIANA_TEST_ASSERT(lines[6] == "stmdb sp!, {r4, r5, r6, r7, r8, r9, sl, fp, lr}");
}

static void test_arm_exe_elf_executable()
{
    ArmExecutable file(g_arm64_app, sizeof(g_arm64_app));
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_ELF);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE64);

    const Diana_ElfFile_impl * pImpl = file.executable.u.elfFile.pImpl;
    DIANA_TEST_ASSERT(pImpl->elfHeader.e_entry == 0x400280);
    DIANA_TEST_ASSERT(pImpl->elfHeader.e_machine == DIANA_EM_AARCH64);

    // _start is at file offset 0x280
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_app, sizeof(g_arm64_app), 0x280, 0x400280) == "mov w0, #0x1");
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_app, sizeof(g_arm64_app), 0x288, 0x400288) == "bl 0x400270");
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_app, sizeof(g_arm64_app), 0x28c, 0x40028c) == "adrp x1, 0x402000");
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_app, sizeof(g_arm64_app), 0x294, 0x400294) == "ldr x2, 0x4002b0");
}

static void test_arm_exe_pe()
{
    ArmExecutable file(g_arm64_pe_dll, sizeof(g_arm64_pe_dll));
    DIANA_TEST_ASSERT(file.executable.type == DIANA_EXECUTABLE_TYPE_PE);
    DIANA_TEST_ASSERT(file.executable.arch == DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(file.executable.dianaMode == DIANA_MODE64);

    const Diana_PeFile_impl * pImpl = file.executable.u.peFile.pImpl;
    DIANA_TEST_ASSERT(pImpl->imageBase == 0x180000000ULL);
    DIANA_TEST_ASSERT(pImpl->addressOfEntryPoint == 0x1000);
    DIANA_TEST_ASSERT(pImpl->capturedSectionCount == 2);     // .text, .pdata
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_pe_dll, sizeof(g_arm64_pe_dll), 0x200, 0x180001000ULL) == "add w0, w0, w1");
    DIANA_TEST_ASSERT(DisassembleAt(file.executable, g_arm64_pe_dll, sizeof(g_arm64_pe_dll), 0x208, 0x180001008ULL) == "mov x0, #0x2a");
}

// ------------------------------------------------------------------ the code analyzer on the fixtures
static DI_UINT64 ReadLE(const unsigned char * p, int size)
{
    DI_UINT64 value = 0;
    for (int i = size - 1; i >= 0; --i)
        value = (value << 8) | p[i];
    return value;
}

// a little-endian ELF64 as loaded at its link addresses: every PT_LOAD at p_vaddr, the image from
// the lowest PT_LOAD page (the link base). Module offsets are the addresses minus the link base
struct MappedElf
{
    std::vector<unsigned char> image;
    OPERAND_SIZE lowest;
    OPERAND_SIZE linkBase;
    std::unique_ptr<TestAnalyzeEnvironment> env;
    Diana_ElfFile elf;
    Diana_InstructionsOwner owner;

    MappedElf(const unsigned char * pFile, size_t size)
        : lowest(~(OPERAND_SIZE)0),
          linkBase(0)
    {
        DI_UINT64 phoff = ReadLE(pFile + 0x20, 8);
        int phentsize = (int)ReadLE(pFile + 0x36, 2);
        int phnum = (int)ReadLE(pFile + 0x38, 2);
        DI_UINT64 end = 0;
        for (int pass = 0; pass < 2; ++pass)
        {
            for (int i = 0; i < phnum; ++i)
            {
                const unsigned char * ph = pFile + phoff + i * phentsize;
                if (ReadLE(ph, 4) != 1)     // PT_LOAD
                    continue;
                DI_UINT64 offset = ReadLE(ph + 0x08, 8), vaddr = ReadLE(ph + 0x10, 8);
                DI_UINT64 filesz = ReadLE(ph + 0x20, 8), memsz = ReadLE(ph + 0x28, 8);
                if (pass == 0)
                {
                    end = std::max(end, vaddr + memsz);
                    lowest = std::min(lowest, (OPERAND_SIZE)vaddr);
                }
                else
                {
                    memcpy(&image[(size_t)(vaddr - linkBase)], pFile + offset, (size_t)std::min(filesz, (DI_UINT64)size - offset));
                }
            }
            if (pass == 0)
            {
                linkBase = lowest & ~(OPERAND_SIZE)0xfff;
                image.resize((size_t)(end - linkBase));
            }
        }
        env.reset(new TestAnalyzeEnvironment((OPERAND_SIZE)&image[0], 0, image.size()));
        env->imageBase = linkBase;     // loaded at its link addresses
        DIANA_TEST_ASSERT(DianaElfFile_Init(&elf, &env->stream, image.size(), DIANA_ELF_FILE_FLAGS_MODULE_MODE) == DI_SUCCESS);
        DIANA_TEST_ASSERT(Diana_ELF_AnalyzeELF(&elf, &env->observer, &owner, 0) == DI_SUCCESS);
    }
    ~MappedElf()
    {
        Diana_InstructionsOwner_Free(&owner);
        DianaElfFile_Free(&elf);
    }
    // a link-time address as a module offset
    OPERAND_SIZE Off(OPERAND_SIZE address) const
    {
        return address - linkBase;
    }
    Diana_Instruction * At(OPERAND_SIZE address)
    {
        Diana_Instruction * pInstruction = owner.m_ppPresenceVec[Off(address)];
        return pInstruction && !(pInstruction->m_flags & DI_INSTRUCTION_INVALID) ? pInstruction : 0;
    }
};

// the exported functions are roots, bl goes through the PLT stub, data in lib_table stays data
static void test_arm_exe_analyze_library()
{
    MappedElf file(g_arm64_libfix_so, sizeof(g_arm64_libfix_so));
    for (OPERAND_SIZE function : {0x220, 0x228, 0x238})
    {
        DIANA_TEST_ASSERT(file.At(function) && (file.At(function)->m_flags & DI_INSTRUCTION_ROOT));
    }
    DIANA_TEST_ASSERT(file.At(0x250) && (file.At(0x250)->m_flags & DI_INSTRUCTION_IS_RET));    // the ret of lib_call
    // lib_table: adr x0, 1f takes the address of two words; 0xdeadbeef is no instruction, so they are data
    DIANA_TEST_ASSERT(!file.At(0x230) && !file.At(0x234));
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x228), 0x230, DI_XREF_DATA|DI_XREF_ADDRESS_TAKEN) != 0);
    // bl lib_add@plt, the stub loads its GOT slot
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x240), 0x210, 0) != 0);
    DIANA_TEST_ASSERT(file.At(0x21c));
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x214), 0x2000, DI_XREF_DATA) != 0);
}

// _start: the PLT stub jumps through .got.plt to PLT0, data in .data and in a literal pool are data xrefs
static void test_arm_exe_analyze_executable()
{
    MappedElf file(g_arm64_app, sizeof(g_arm64_app));
    DIANA_TEST_ASSERT(file.At(0x400280) && (file.At(0x400280)->m_flags & DI_INSTRUCTION_ROOT));
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x400288), file.Off(0x400270), 0) != 0);         // bl lib_add@plt
    // the slot of lib_add holds PLT0 until the dynamic linker binds it
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x400274), file.Off(0x402000), DI_XREF_DATA) != 0);
    Diana_XRef * pJump = FindXRefFrom(file.At(0x40027c), file.Off(0x400250), 0);
    DIANA_TEST_ASSERT(pJump && !(pJump->m_flags & (DI_XREF_DATA|DI_XREF_INVALID)));
    DIANA_TEST_ASSERT(file.At(0x400250) && file.At(0x400260));
    // PLT0 loads a slot that is 0 in the file: no target
    DIANA_TEST_ASSERT(Diana_FList_First(&file.At(0x400260)->m_referencesFromThisInstruction) == 0);
    // adrp x1, value; ldr x1, [x1, #8]: .data; ldr x2, =0x1122334455667788: the literal pool
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x400290), file.Off(0x402008), DI_XREF_DATA) != 0);
    DIANA_TEST_ASSERT(FindXRefFrom(file.At(0x400294), file.Off(0x4002b0), DI_XREF_DATA) != 0);
    DIANA_TEST_ASSERT(!file.At(0x4002b0) && !file.At(0x4002b4));
}

// a PE32+ ARM64 image in memory: the entry point and .pdata are roots
static void test_arm_exe_analyze_pe()
{
    // map the file: headers, then every section at its RVA
    const unsigned char * pFile = g_arm64_pe_dll;
    const unsigned char * pNt = pFile + ReadLE(pFile + 0x3c, 4);
    int sections = (int)ReadLE(pNt + 6, 2);
    int optionalSize = (int)ReadLE(pNt + 0x14, 2);
    const unsigned char * pOptional = pNt + 0x18;
    std::vector<unsigned char> image((size_t)ReadLE(pOptional + 56, 4));
    memcpy(&image[0], pFile, (size_t)ReadLE(pOptional + 60, 4));
    for (int i = 0; i < sections; ++i)
    {
        const unsigned char * pSection = pOptional + optionalSize + i * 40;
        memcpy(&image[(size_t)ReadLE(pSection + 12, 4)], pFile + ReadLE(pSection + 20, 4), (size_t)ReadLE(pSection + 16, 4));
    }

    Diana_InstructionsOwner owner;
    DIANA_TEST_ASSERT(Diana_PE_AnalyzePEInMemory(&image[0], image.size(), &owner, 0) == DI_SUCCESS);
    // add w0, w0, w1; ret is the entry point, mov x0, #42; ret only in .pdata
    for (size_t function : {0x1000, 0x1008})
    {
        Diana_Instruction * pInstruction = owner.m_ppPresenceVec[function];
        DIANA_TEST_ASSERT(pInstruction && (pInstruction->m_flags & DI_INSTRUCTION_ROOT) &&
                          !(pInstruction->m_flags & DI_INSTRUCTION_INVALID));
        DIANA_TEST_ASSERT(owner.m_ppPresenceVec[function + 4] != 0);
    }
    DIANA_TEST_ASSERT(owner.m_actualSize == 4);
    Diana_InstructionsOwner_Free(&owner);
}

void test_arm_exe()
{
    DIANA_TEST(test_arm_exe_elf_arm32());
    DIANA_TEST(test_arm_exe_pe_armnt());
    DIANA_TEST(test_arm_exe_elf_library());
    DIANA_TEST(test_arm_exe_elf_big_endian());
    DIANA_TEST(test_arm_exe_elf_arm_be32());
    DIANA_TEST(test_arm_exe_elf_executable());
    DIANA_TEST(test_arm_exe_pe());
    DIANA_TEST(test_arm_exe_analyze_library());
    DIANA_TEST(test_arm_exe_analyze_executable());
    DIANA_TEST(test_arm_exe_analyze_pe());
}
