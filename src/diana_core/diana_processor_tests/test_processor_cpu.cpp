// The instructions the state compare of tests/x86/cpu_compare found wrong and the ones added after it (POPCNT ...
// BMI, SSE3, SSE4, AES, SHA, the VEX.128 forms), with the CPU's results: every row of
// test_processor_cpu.inc is the state after one instruction on an Intel Core i7-11800H (cpu_compare --emit-unit), from
// the fixed state of tests/x86/cpu_state.cpp, which this file builds the same way (the region at 0x30000000, the
// registers, the data, the x87 stack of the init). Compared: the general registers, the arithmetic flags and DF,
// the x87 control and status words, the tags, st0 and st1 when valid, MXCSR, ymm1, 40 bytes of data.
#include "test_common.h"
#include <string.h>
#include <stdio.h>
extern "C"
{
#include "diana_streams.h"
#include "diana_gen.h"
#include "diana_processor.h"
#include "diana_allocators.h"
#include "diana_processor/diana_processor_core_impl.h"
#include "diana_processor/diana_processor_core_impl_xmm.h"
#include "diana_processor/diana_processor_cmd_fpu_internal.h"
}

namespace
{

const OPERAND_SIZE kBase = 0x30000000;
const OPERAND_SIZE kSize = 0x10000;
const OPERAND_SIZE kInsn = kBase + 0x100;
const OPERAND_SIZE kData = kBase + 0x1000;
const OPERAND_SIZE kStackTop = kBase + 0x2800;
const OPERAND_SIZE kStackLow = kStackTop - 0x80;

const unsigned kArithFlags = 0x8d5;  // OF SF ZF AF PF CF
const unsigned kFlagDF = 0x400;

struct CpuRow
{
    const char * name;
    int mode;
    unsigned char bytes[16];
    int size;
    int init;
    unsigned undefinedFlags;
    unsigned long long gpr[16];
    unsigned long long rflags;
    unsigned short fcw, fsw;
    unsigned char ftw;
    unsigned char st0[10], st1[10];
    unsigned int mxcsr;
    unsigned char xmm1[16];
    unsigned char ymm1High[16];     // bits 255:128
    unsigned char data[40];
};

const CpuRow kRows[] = {
#include "test_processor_cpu.inc"
};

// the fixed state: an fxsave image (FCW 0, FSW 2, FTW 4, MXCSR 24, ST(i) 32 + 16 i, xmm 160 + 16 i), the general
// registers, the flags, 256 bytes of data at kData, 256 bytes of stack at kStackLow
struct State
{
    unsigned char fx[512];
    unsigned char ymmHigh[256];     // bits 255:128 of ymm i at 16 i
    unsigned long long gpr[16];
    unsigned long long rflags;
    unsigned char data[256];
    unsigned char stack[256];
};

void BaseState(State & s)
{
    memset(&s, 0, sizeof(s));
    const unsigned long long gpr[16] = {0x0123456789abcdefULL, ~0ULL, 0x40, kData + 0x80, kStackTop,
                                        0xfedcba9876543210ULL, 0x00000000deadbeefULL, 0x8000000000000000ULL,
                                        1, 0x7fffffff, 0x80000000ULL, 0xffffffff00000000ULL, 0x5555555555555555ULL,
                                        0xaaaaaaaaaaaaaaaaULL, 0, 0x100};
    memcpy(s.gpr, gpr, sizeof(gpr));
    s.rflags = 0x202;
    unsigned short fcw = 0x37f;
    memcpy(s.fx, &fcw, 2);
    unsigned int mxcsr = 0x1f80;
    memcpy(s.fx + 24, &mxcsr, 4);
    const float xmm1[4] = {10, 20, 30, 40};
    memcpy(s.fx + 160 + 16, xmm1, 16);
    for (int i = 2; i < 16; ++i)
        for (int j = 0; j < 16; ++j)
            s.fx[160 + 16 * i + j] = (unsigned char)(i * 16 + j);
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j)
            s.ymmHigh[16 * i + j] = (unsigned char)(0xa0 + i + j * 3);
    for (int i = 0; i < 256; ++i)
    {
        s.data[i] = (unsigned char)(i * 37 + 11);
        s.stack[i] = (unsigned char)(i * 53 + 7);
    }
    const unsigned int words[4] = {0x80030201, 0x80030201, 0x8f5c28f6, 0x3ff4f5c2};
    memcpy(s.data + 0x80, words, 16);
}

// pushes an 80-bit value (the significand, the sign and exponent) on the x87 stack of the image
void FpuPush(State & s, unsigned long long fraction, unsigned short exponent)
{
    unsigned short fsw;
    memcpy(&fsw, s.fx + 2, 2);
    int top = (((fsw >> 11) & 7) + 7) & 7;
    fsw = (unsigned short)((fsw & ~0x3800) | (top << 11));
    memcpy(s.fx + 2, &fsw, 2);
    memmove(s.fx + 48, s.fx + 32, 16 * 7);
    memset(s.fx + 32, 0, 16);
    memcpy(s.fx + 32, &fraction, 8);
    memcpy(s.fx + 40, &exponent, 2);
    s.fx[4] |= (unsigned char)(1 << top);
}

// the run 0 of the inits of tests/x86/cpu_state.cpp (CpuRow::init)
void Init(State & s, int init)
{
    switch (init)
    {
    case 1:     // InitFpu2: 2.5, 1.25
        FpuPush(s, 0xa000000000000000ULL, 0x4000);
        FpuPush(s, 0xa000000000000000ULL, 0x3fff);
        break;
    case 2:     // InitFpuSmall: 3, 0.5
        FpuPush(s, 0xc000000000000000ULL, 0x4000);
        FpuPush(s, 0x8000000000000000ULL, 0x3ffe);
        break;
    case 3:     // InitMmx: mmi = 0x8070605040302010 + 0x0101010101010101 i, every register valid
        for (int i = 0; i < 8; ++i)
        {
            unsigned long long value = 0x8070605040302010ULL + 0x0101010101010101ULL * (unsigned)i;
            memcpy(s.fx + 32 + 16 * i, &value, 8);
            s.fx[40 + 16 * i] = 0xff;
            s.fx[41 + 16 * i] = 0xff;
        }
        s.fx[4] = 0xff;
        break;
    case 4:     // InitFpuBig: 3, 1000.5 * 2^70
        FpuPush(s, 0xc000000000000000ULL, 0x4000);
        FpuPush(s, 0xfa20000000000000ULL, 0x4008 + 70);
        break;
    case 5:     // InitLdmxcsr: [rbx+0x20] = MXCSR, rounding down
    {
        unsigned int mxcsr = 0x3f80;
        memcpy(s.data + 0xa0, &mxcsr, 4);
        break;
    }
    case 6:     // InitFldcw: [rbx+0x22] = FCW, rounding down, 64 bits
    {
        unsigned short fcw = 0x077f;
        memcpy(s.data + 0xa2, &fcw, 2);
        break;
    }
    case 7:     // InitFpuNegative: -2
        FpuPush(s, 0x8000000000000000ULL, 0xc000);
        break;
    case 8:     // InitUnitDoubles: xmm2 = -2.75, 1e300; [rbx+0x10] = -1.5, 3e9
    {
        const double xmm2[2] = {-2.75, 1e300}, data[2] = {-1.5, 3e9};
        memcpy(s.fx + 160 + 32, xmm2, 16);
        memcpy(s.data + 0x90, data, 16);
        break;
    }
    case 9:     // InitMaskMove: InitMmx, rdi = the data + 0x40
        Init(s, 3);
        s.gpr[7] = kData + 0x40;
        break;
    case 10:    // InitStrings: xmm1 "bc", xmm2 and [rbx] "xxabcabcbcbz", rax 3, rdx 10
    {
        const char a[16] = "bc", b[16] = "xxabcabcbcbz";
        memcpy(s.fx + 160 + 16, a, 16);
        memcpy(s.fx + 160 + 32, b, 16);
        memcpy(s.data + 0x80, b, 16);
        s.gpr[0] = 3;
        s.gpr[2] = 10;
        break;
    }
    case 11:    // InitNaNFloats: xmm1 the QNaNs 5, 6, 7, 8, xmm2 ones
        for (int i = 0; i < 4; ++i)
        {
            unsigned int nan = 0x7fc00005u + (unsigned)i, one = 0x3f800000u;
            memcpy(s.fx + 160 + 16 + 4 * i, &nan, 4);
            memcpy(s.fx + 160 + 32 + 4 * i, &one, 4);
        }
        break;
    case 12:    // InitNaNDoubles: xmm1 the QNaNs 5, 6, xmm2 ones
        for (int i = 0; i < 2; ++i)
        {
            unsigned long long nan = 0x7ff8000000000005ULL + (unsigned)i, one = 0x3ff0000000000000ULL;
            memcpy(s.fx + 160 + 16 + 8 * i, &nan, 8);
            memcpy(s.fx + 160 + 32 + 8 * i, &one, 8);
        }
        break;
    default:
        break;
    }
}

// the memory of the processor: kSize bytes at kBase
struct RegionStream
{
    DianaRandomReadWriteStream parent;
    unsigned char * buffer;
};

int RegionReadWrite(void * pThis, OPERAND_SIZE offset, void * pBuffer, OPERAND_SIZE size, OPERAND_SIZE * done,
                    int write)
{
    RegionStream * pStream = (RegionStream *)pThis;
    *done = 0;
    if (offset < kBase || offset >= kBase + kSize)
        return DI_ERROR;
    OPERAND_SIZE give = kBase + kSize - offset;
    if (give > size)
        give = size;
    if (write)
        memcpy(pStream->buffer + (offset - kBase), pBuffer, (size_t)give);
    else
        memcpy(pBuffer, pStream->buffer + (offset - kBase), (size_t)give);
    *done = give;
    return DI_SUCCESS;
}

int RegionRead(void * pThis, OPERAND_SIZE, OPERAND_SIZE offset, void * pBuffer, OPERAND_SIZE size,
               OPERAND_SIZE * done, struct _dianaProcessor *, DianaUnifiedRegister)
{
    return RegionReadWrite(pThis, offset, pBuffer, size, done, 0);
}

int RegionWrite(void * pThis, OPERAND_SIZE, OPERAND_SIZE offset, void * pBuffer, OPERAND_SIZE size,
                OPERAND_SIZE * done, struct _dianaProcessor *, DianaUnifiedRegister)
{
    return RegionReadWrite(pThis, offset, pBuffer, size, done, 1);
}

const DianaUnifiedRegister kGprRegs[16] = {reg_RAX, reg_RCX, reg_RDX, reg_RBX, reg_RSP, reg_RBP, reg_RSI, reg_RDI,
                                           reg_R8, reg_R9, reg_R10, reg_R11, reg_R12, reg_R13, reg_R14, reg_R15};

// runs the row in DianaProcessor; the number of differences (printed)
int RunRow(const CpuRow & row)
{
    State s;
    BaseState(s);
    Init(s, row.init);
    const int count = row.mode == 64 ? 16 : 8;
    if (row.mode == 32)
        for (int i = 0; i < 16; ++i)
            s.gpr[i] = i < 8 ? s.gpr[i] & 0xffffffffULL : 0;

    unsigned char * memory = new unsigned char[(size_t)kSize];
    memset(memory, 0, (size_t)kSize);
    memcpy(memory + (kInsn - kBase), row.bytes, row.size);
    memcpy(memory + (kData - kBase), s.data, 256);
    memcpy(memory + (kStackLow - kBase), s.stack, 256);
    RegionStream stream;
    stream.parent.pReadFnc = RegionRead;
    stream.parent.pWriteFnc = RegionWrite;
    stream.buffer = memory;
    DianaMAllocator allocator;
    Diana_InitMAllocator(&allocator);
    DianaProcessor proc;
    int res = DianaProcessor_Init(&proc, &stream.parent, &allocator.m_parent,
                                  row.mode == 64 ? DIANA_MODE64 : DIANA_MODE32);
    DIANA_TEST_ASSERT(res == DI_SUCCESS);
    DianaProcessor * p = &proc;
    for (int i = 0; i < count; ++i)
        DianaProcessor_SetValue(p, kGprRegs[i], DianaProcessor_QueryReg(p, kGprRegs[i]), s.gpr[i]);
    DianaProcessor_SetValue(p, reg_RIP, DianaProcessor_QueryReg(p, reg_RIP), kInsn);
    p->m_flags.value = s.rflags;
    for (int i = 0; i < count; ++i)
    {
        DianaRegisterXMM_type value;
        memcpy(&value, s.fx + 160 + 16 * i, 16);
        DianaUnifiedRegister reg = (DianaUnifiedRegister)(reg_XMM0 + i);
        DianaProcessor_XMM_SetValue(p, reg, DianaProcessor_QueryReg(p, reg), &value);
        DianaRegisterYMM_type ymm = DianaProcessor_YMM_GetValue(p, reg);
        memcpy(&ymm.xmm[1], s.ymmHigh + 16 * i, 16);
        DianaProcessor_YMM_SetValue(p, reg, &ymm);
    }
    memcpy(&p->m_fpu.controlWord, s.fx, 2);
    memcpy(&p->m_fpu.statusWord, s.fx + 2, 2);
    memcpy(&p->m_mxcsr, s.fx + 24, 4);
    for (int i = 0; i < 8; ++i)
    {
        floatx80_t value;
        memcpy(&value.fraction, s.fx + 32 + 16 * i, 8);
        memcpy(&value.exp, s.fx + 40 + 16 * i, 2);
        DianaProcessor_FPU_SetSTRegister_80(p, (DianaUnifiedRegister)(reg_fpu_ST0 + i), &value);
    }
    for (int i = 0; i < 8; ++i)
        p->m_fpu.registerFlags[i] = (s.fx[4] >> i) & 1 ? DI_PROCESSOR_FPU_REGISTER_BUSY : 0;

    res = DianaProcessor_ExecOnce(p);

    int bad = 0;
    if (res != DI_SUCCESS)
    {
        printf("%s (%d-bit): status %d\n", row.name, row.mode, res);
        ++bad;
    }
    for (int i = 0; i < count && !bad; ++i)
    {
        OPERAND_SIZE value = DianaProcessor_GetValue(p, DianaProcessor_QueryReg(p, kGprRegs[i]));
        if (row.mode == 32)
            value &= 0xffffffffULL;
        if (value != row.gpr[i])
        {
            printf("%s (%d-bit): gpr %d %016llx, the cpu %016llx\n", row.name, row.mode, i,
                   (unsigned long long)value, row.gpr[i]);
            ++bad;
        }
    }
    unsigned mask = (kArithFlags | kFlagDF) & ~row.undefinedFlags;
    if (!bad && ((p->m_flags.value ^ row.rflags) & mask))
    {
        printf("%s (%d-bit): flags %03llx, the cpu %03llx\n", row.name, row.mode,
               (unsigned long long)(p->m_flags.value & mask), row.rflags & mask);
        ++bad;
    }
    unsigned char ftw = 0;
    for (int i = 0; i < 8; ++i)
        if (p->m_fpu.registerFlags[i] & DI_PROCESSOR_FPU_REGISTER_BUSY)
            ftw |= (unsigned char)(1 << i);
    if (!bad && (p->m_fpu.controlWord != row.fcw || p->m_fpu.statusWord != row.fsw || ftw != row.ftw ||
                 p->m_mxcsr != row.mxcsr))
    {
        printf("%s (%d-bit): fcw %04x fsw %04x tags %02x mxcsr %08x, the cpu %04x %04x %02x %08x\n", row.name,
               row.mode, p->m_fpu.controlWord, p->m_fpu.statusWord, ftw, p->m_mxcsr, row.fcw, row.fsw, row.ftw,
               row.mxcsr);
        ++bad;
    }
    int top = (row.fsw >> 11) & 7;
    for (int i = 0; i < 2 && !bad; ++i)
    {
        if (!((row.ftw >> ((top + i) & 7)) & 1))
            continue;
        floatx80_t value;
        DianaProcessor_FPU_GetSTRegister_80(p, (DianaUnifiedRegister)(reg_fpu_ST0 + i), &value);
        unsigned char bytes[10];
        memcpy(bytes, &value.fraction, 8);
        memcpy(bytes + 8, &value.exp, 2);
        if (memcmp(bytes, i ? row.st1 : row.st0, 10))
        {
            printf("%s (%d-bit): st%d differs from the cpu\n", row.name, row.mode, i);
            ++bad;
        }
    }
    DianaRegisterXMM_type xmm1 = DianaProcessor_XMM_GetValue(p, DianaProcessor_QueryReg(p, reg_XMM1));
    if (!bad && memcmp(&xmm1, row.xmm1, 16))
    {
        printf("%s (%d-bit): xmm1 differs from the cpu\n", row.name, row.mode);
        ++bad;
    }
    DianaRegisterYMM_type ymm1 = DianaProcessor_YMM_GetValue(p, reg_YMM1);
    if (!bad && memcmp(&ymm1.xmm[1], row.ymm1High, 16))
    {
        printf("%s (%d-bit): ymm1 bits 255:128 differ from the cpu\n", row.name, row.mode);
        ++bad;
    }
    if (!bad && memcmp(memory + (kData - kBase) + 0x80, row.data, 40))
    {
        printf("%s (%d-bit): the data differs from the cpu\n", row.name, row.mode);
        ++bad;
    }
    DianaProcessor_Free(p);
    delete[] memory;
    return bad;
}

}

static void test_processor_cpu_rows()
{
    int bad = 0;
    for (size_t i = 0; i < sizeof(kRows) / sizeof(kRows[0]); ++i)
        bad += RunRow(kRows[i]);
    DIANA_TEST_ASSERT(!bad);
}

void test_processor_cpu()
{
    DIANA_TEST(test_processor_cpu_rows());
}
