#include "test_common.h"
#include "orthia_diana_arch.h"
#include "orthia_files.h"
#include "orthia_utils.h"
extern "C"
{
#include "diana_pe_defs.h"
#include "diana_elfs_defs.h"
}

// The architecture of a live process comes from what the OS reports: IsWow64Process2 and
// ProcessMachineTypeInfo on Windows, the ELF header of /proc/<pid>/exe on Linux. The rules are tested
// here for every host, an ARM machine included, without one

static bool PeMachineIs(int machine, int expectedMode, int expectedArch)
{
    int mode = -1;
    int arch = -1;
    if (!orthia::FindDianaArchOfPeMachine(machine, &mode, &arch))
    {
        return false;
    }
    return mode == expectedMode && arch == expectedArch;
}

static void test_process_arch_pe_machine()
{
    DIANA_TEST_ASSERT(PeMachineIs(DIANA_IMAGE_FILE_MACHINE_I386, DIANA_MODE32, DIANA_ARCH_X86));
    DIANA_TEST_ASSERT(PeMachineIs(DIANA_IMAGE_FILE_MACHINE_AMD64, DIANA_MODE64, DIANA_ARCH_X86));
    DIANA_TEST_ASSERT(PeMachineIs(DIANA_IMAGE_FILE_MACHINE_ARM64, DIANA_MODE64, DIANA_ARCH_AARCH64));
    DIANA_TEST_ASSERT(PeMachineIs(DIANA_IMAGE_FILE_MACHINE_ARMNT, DIANA_MODE32, DIANA_ARCH_THUMB));
    DIANA_TEST_ASSERT(PeMachineIs(DIANA_IMAGE_FILE_MACHINE_THUMB, DIANA_MODE32, DIANA_ARCH_THUMB));

    // unknown machines leave the outputs alone
    int mode = 7;
    int arch = 7;
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfPeMachine(0, &mode, &arch));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfPeMachine(DIANA_IMAGE_FILE_MACHINE_IA64, &mode, &arch));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfPeMachine(DIANA_IMAGE_FILE_MACHINE_ARM64EC, &mode, &arch));
    DIANA_TEST_ASSERT(mode == 7 && arch == 7);
}

// what IsWow64Process2 and ProcessMachineTypeInfo report, as documented, for each kind of process
static void test_process_arch_windows_machine()
{
    const int unknown = 0;
    const int i386 = DIANA_IMAGE_FILE_MACHINE_I386;
    const int amd64 = DIANA_IMAGE_FILE_MACHINE_AMD64;
    const int arm64 = DIANA_IMAGE_FILE_MACHINE_ARM64;
    const int armnt = DIANA_IMAGE_FILE_MACHINE_ARMNT;

    // x64 Windows: a native process and a WOW64 one
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(amd64, unknown, amd64) == amd64);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(i386, i386, amd64) == i386);
    // Windows 10 x64 before ProcessMachineTypeInfo
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, unknown, amd64) == amd64);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, i386, amd64) == i386);

    // ARM64 Windows 11: native, x86 and ARM32 under WOW64, x64 under emulation
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(arm64, unknown, arm64) == arm64);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(i386, i386, arm64) == i386);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(armnt, armnt, arm64) == armnt);
    // IsWow64Process2 says native for an emulated x64 process, ProcessMachineTypeInfo tells
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(amd64, unknown, arm64) == amd64);

    // ARM64 Windows 10: no ProcessMachineTypeInfo
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, unknown, arm64) == arm64);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, i386, arm64) == i386);
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, armnt, arm64) == armnt);

    // Windows before 10 1709: nothing, the caller falls back to IsWow64Process
    DIANA_TEST_ASSERT(orthia::SelectWin32ProcessMachine(unknown, unknown, unknown) == unknown);

    // and what each comes to
    int mode = 0;
    int arch = 0;
    DIANA_TEST_ASSERT(orthia::FindDianaArchOfPeMachine(orthia::SelectWin32ProcessMachine(arm64, unknown, arm64), &mode, &arch));
    DIANA_TEST_ASSERT(mode == DIANA_MODE64 && arch == DIANA_ARCH_AARCH64);
    DIANA_TEST_ASSERT(orthia::FindDianaArchOfPeMachine(orthia::SelectWin32ProcessMachine(armnt, armnt, arm64), &mode, &arch));
    DIANA_TEST_ASSERT(mode == DIANA_MODE32 && arch == DIANA_ARCH_THUMB);
    DIANA_TEST_ASSERT(orthia::FindDianaArchOfPeMachine(orthia::SelectWin32ProcessMachine(amd64, unknown, arm64), &mode, &arch));
    DIANA_TEST_ASSERT(mode == DIANA_MODE64 && arch == DIANA_ARCH_X86);
}

static std::vector<unsigned char> MakeElfHeader(int elfClass, int data, int machine)
{
    std::vector<unsigned char> header(64, 0);
    header[0] = 0x7f;
    header[1] = 'E';
    header[2] = 'L';
    header[3] = 'F';
    header[DIANA_EI_CLASS] = (unsigned char)elfClass;
    header[DIANA_EI_DATA] = (unsigned char)data;
    if (data == DIANA_ELFDATA2MSB)
    {
        header[18] = (unsigned char)(machine >> 8);
        header[19] = (unsigned char)machine;
    }
    else
    {
        header[18] = (unsigned char)machine;
        header[19] = (unsigned char)(machine >> 8);
    }
    return header;
}

static bool ElfHeaderIs(const std::vector<unsigned char>& header, size_t size, int expectedMode, int expectedArch)
{
    int mode = -1;
    int arch = -1;
    if (!orthia::FindDianaArchOfElfHeader(header.data(), size, &mode, &arch))
    {
        return false;
    }
    return mode == expectedMode && arch == expectedArch;
}

static void test_process_arch_elf_header()
{
    const int lsb = DIANA_ELFDATA2LSB;
    const int msb = DIANA_ELFDATA2MSB;
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS64, lsb, DIANA_EM_X86_64), 64, DIANA_MODE64, DIANA_ARCH_X86));
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS32, lsb, DIANA_EM_386), 64, DIANA_MODE32, DIANA_ARCH_X86));
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS64, lsb, DIANA_EM_AARCH64), 64, DIANA_MODE64, DIANA_ARCH_AARCH64));
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS32, lsb, DIANA_EM_ARM), 64, DIANA_MODE32, DIANA_ARCH_ARM32));
    // aarch64_be: e_machine in big-endian
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS64, msb, DIANA_EM_AARCH64), 64, DIANA_MODE64, DIANA_ARCH_AARCH64));
    // x32 is ELF32 with EM_X86_64
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS32, lsb, DIANA_EM_X86_64), 64, DIANA_MODE32, DIANA_ARCH_X86));
    // e_machine is the last thing read
    DIANA_TEST_ASSERT(ElfHeaderIs(MakeElfHeader(DIANA_ELFCLASS64, lsb, DIANA_EM_AARCH64), 20, DIANA_MODE64, DIANA_ARCH_AARCH64));

    // not taken, the outputs stay
    int mode = 7;
    int arch = 7;
    auto header = MakeElfHeader(DIANA_ELFCLASS64, lsb, DIANA_EM_AARCH64);
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), 19, &mode, &arch));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), 5, &mode, &arch));
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), 0, &mode, &arch));
    // RISC-V (243), a machine orthia doesn't decode
    header = MakeElfHeader(DIANA_ELFCLASS64, lsb, 243);
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), header.size(), &mode, &arch));
    header = MakeElfHeader(3, lsb, DIANA_EM_AARCH64);
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), header.size(), &mode, &arch));
    header = MakeElfHeader(DIANA_ELFCLASS64, 3, DIANA_EM_AARCH64);
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), header.size(), &mode, &arch));
    header = MakeElfHeader(DIANA_ELFCLASS64, lsb, DIANA_EM_AARCH64);
    header[0] = 'M';
    DIANA_TEST_ASSERT(!orthia::FindDianaArchOfElfHeader(header.data(), header.size(), &mode, &arch));
    DIANA_TEST_ASSERT(mode == 7 && arch == 7);
}

// data/ next to the source tree, as the ELF tests find it
static orthia::PlatformString_type ProcessArchDataPath(const orthia::PlatformString_type& name)
{
    auto moduleDir = orthia::GetCurrentProcessDir();
#ifdef DIANA_HAS_WIN32
    return moduleDir + ORTHIA_TCSTR("../../../data/") + name;
#else
    return moduleDir + ORTHIA_TCSTR("../../../../data/") + name;
#endif
}

// the first bytes of the committed test images, what /proc/<pid>/exe would give
static void test_process_arch_elf_files()
{
    struct Case
    {
        const ORTHIA_TCHAR* name;
        int mode;
        int arch;
    };
    const Case cases[] = {
        { ORTHIA_TCSTR("elf/libstartup_x64.so"), DIANA_MODE64, DIANA_ARCH_X86 },
        { ORTHIA_TCSTR("elf/libstartup_arm64.so"), DIANA_MODE64, DIANA_ARCH_AARCH64 },
        { ORTHIA_TCSTR("elf/libstartup_arm32.so"), DIANA_MODE32, DIANA_ARCH_ARM32 },
        // A32 or T32 is told per module, the process is ARM32 either way
        { ORTHIA_TCSTR("elf/libstartup_thumb.so"), DIANA_MODE32, DIANA_ARCH_ARM32 },
    };
    for (const auto& item : cases)
    {
        std::vector<char> data;
        orthia::LoadFileToVector(ProcessArchDataPath(item.name), data);
        DIANA_TEST_ASSERT(data.size() >= 20);
        int mode = 0;
        int arch = 0;
        DIANA_TEST_ASSERT(orthia::FindDianaArchOfElfHeader((const unsigned char*)data.data(), 20, &mode, &arch));
        DIANA_TEST_ASSERT(mode == item.mode && arch == item.arch);
    }
}

void test_process_arch()
{
    DIANA_TEST(test_process_arch_pe_machine());
    DIANA_TEST(test_process_arch_windows_machine());
    DIANA_TEST(test_process_arch_elf_header());
    DIANA_TEST(test_process_arch_elf_files());
}
