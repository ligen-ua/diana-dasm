#include "diana_processor_cmd_crypto.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

// AES-NI (aesenc, aesenclast, aesdec, aesdeclast, aesimc, aeskeygenassist), PCLMULQDQ, SHA (sha1rnds4, sha1nexte,
// sha1msg1, sha1msg2, sha256msg1, sha256msg2) after the pseudo-code of the SDM. The AES state is the 16 bytes of
// xmm, column by column (byte r + 4 c is row r, column c), as FIPS-197 lays out its input.

// ---------------------------------------------------------------------------------------------------------------
// operands

static int Di_CryptoGet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                        DianaRegisterXMM_type * pValue)
{
    int size = 16;
    DIANA_MEMSET(pValue, 0, sizeof(*pValue));
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 0, &size, 0, 1);
}

static int Di_CryptoSet(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                        DianaRegisterXMM_type * pValue)
{
    int size = 16;
    return DianaProcessor_XMM_SetGetOperandEx(pDianaContext, pCallContext, number, pValue, 1, &size, 0, 1);
}

static int Di_CryptoImm(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, int number,
                        OPERAND_SIZE * pImm)
{
    int size = 0;
    return DianaProcessor_SetGetOperand(pDianaContext, pCallContext, number, pImm, 0, &size, 0);
}

// ---------------------------------------------------------------------------------------------------------------
// AES

static DI_CHAR g_sbox[256], g_inverseSbox[256];
static int g_sboxReady;

// multiplication in GF(2^8) mod x^8 + x^4 + x^3 + x + 1
static DI_CHAR Di_GfMul(DI_CHAR a, DI_CHAR b)
{
    DI_CHAR product = 0;
    while (b)
    {
        if (b & 1)
            product ^= a;
        a = (DI_CHAR)((a << 1) ^ (a & 0x80 ? 0x1b : 0));
        b >>= 1;
    }
    return product;
}

// the S-box: the inverse in GF(2^8) (x^254), then the affine transformation
static void Di_InitSbox()
{
    int x, i;
    if (g_sboxReady)
        return;
    for (x = 0; x < 256; ++x)
    {
        DI_CHAR inverse = 1, power = (DI_CHAR)x, s = 0;
        int exponent = 254;
        if (!x)
            inverse = 0;
        else
            for (; exponent; exponent >>= 1, power = Di_GfMul(power, power))
                if (exponent & 1)
                    inverse = Di_GfMul(inverse, power);
        s = inverse;
        for (i = 1; i <= 4; ++i)
            s ^= (DI_CHAR)((inverse << i) | (inverse >> (8 - i)));
        s ^= 0x63;
        g_sbox[x] = s;
        g_inverseSbox[s] = (DI_CHAR)x;
    }
    g_sboxReady = 1;
}

static void Di_SubBytes(DI_CHAR * state, const DI_CHAR * box)
{
    int i;
    for (i = 0; i < 16; ++i)
        state[i] = box[state[i]];
}

// row r rotates left by r (ShiftRows) or right by r (InvShiftRows)
static void Di_ShiftRows(DI_CHAR * state, int inverse)
{
    DI_CHAR copy[16];
    int r, c;
    DIANA_MEMCPY(copy, state, 16);
    for (r = 0; r < 4; ++r)
        for (c = 0; c < 4; ++c)
            state[r + 4 * c] = copy[r + 4 * (inverse ? (c + 4 - r) % 4 : (c + r) % 4)];
}

static void Di_MixColumns(DI_CHAR * state, int inverse)
{
    static const DI_CHAR forward[4] = {2, 3, 1, 1}, backward[4] = {14, 11, 13, 9};
    const DI_CHAR * m = inverse ? backward : forward;
    DI_CHAR column[4];
    int r, c, k;
    for (c = 0; c < 4; ++c)
    {
        DIANA_MEMCPY(column, state + 4 * c, 4);
        for (r = 0; r < 4; ++r)
        {
            DI_CHAR value = 0;
            for (k = 0; k < 4; ++k)
                value ^= Di_GfMul(m[(k - r + 4) % 4], column[k]);
            state[r + 4 * c] = value;
        }
    }
}

void Diana_AesRoundValue(DianaRegisterXMM_type * pState, const DianaRegisterXMM_type * pKey, DiAesRound round)
{
    int i;
    Di_InitSbox();
    if (round == DI_AESENC || round == DI_AESENCLAST)
    {
        Di_ShiftRows(pState->u8, 0);
        Di_SubBytes(pState->u8, g_sbox);
        if (round == DI_AESENC)
            Di_MixColumns(pState->u8, 0);
    }
    else
    {
        Di_ShiftRows(pState->u8, 1);
        Di_SubBytes(pState->u8, g_inverseSbox);
        if (round == DI_AESDEC)
            Di_MixColumns(pState->u8, 1);
    }
    for (i = 0; i < 16; ++i)
        pState->u8[i] ^= pKey->u8[i];
}

static int Di_AesRound(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext, DiAesRound round)
{
    DianaRegisterXMM_type state, key;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &state));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &key));
    Diana_AesRoundValue(&state, &key, round);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &state));
    DI_PROC_END
}

int Diana_Call_aesenc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AesRound(pDianaContext, pCallContext, DI_AESENC);
}
int Diana_Call_aesenclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AesRound(pDianaContext, pCallContext, DI_AESENCLAST);
}
int Diana_Call_aesdec(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AesRound(pDianaContext, pCallContext, DI_AESDEC);
}
int Diana_Call_aesdeclast(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    return Di_AesRound(pDianaContext, pCallContext, DI_AESDECLAST);
}

void Diana_AesImcValue(DianaRegisterXMM_type * pValue)
{
    Di_MixColumns(pValue->u8, 1);
}

// aesimc: InvMixColumns of the source
int Diana_Call_aesimc(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type value;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &value));
    Diana_AesImcValue(&value);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &value));
    DI_PROC_END
}

static DI_UINT32 Di_SubWord(DI_UINT32 word)
{
    return (DI_UINT32)g_sbox[word & 0xff] | (DI_UINT32)g_sbox[(word >> 8) & 0xff] << 8 |
           (DI_UINT32)g_sbox[(word >> 16) & 0xff] << 16 | (DI_UINT32)g_sbox[word >> 24] << 24;
}

static DI_UINT32 Di_RotWord(DI_UINT32 word)
{
    return (word >> 8) | (word << 24);
}

void Diana_AesKeyGenAssistValue(DianaRegisterXMM_type * pDest, const DianaRegisterXMM_type * pSrc, int rcon)
{
    Di_InitSbox();
    pDest->u32[0] = Di_SubWord(pSrc->u32[1]);
    pDest->u32[1] = Di_RotWord(Di_SubWord(pSrc->u32[1])) ^ (DI_UINT32)(rcon & 0xff);
    pDest->u32[2] = Di_SubWord(pSrc->u32[3]);
    pDest->u32[3] = Di_RotWord(Di_SubWord(pSrc->u32[3])) ^ (DI_UINT32)(rcon & 0xff);
}

// aeskeygenassist: SubWord(X1), RotWord(SubWord(X1)) ^ RCON, SubWord(X3), RotWord(SubWord(X3)) ^ RCON
int Diana_Call_aeskeygenassist(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type src, dest;
    OPERAND_SIZE imm = 0;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    DI_CHECK(Di_CryptoImm(pDianaContext, pCallContext, 2, &imm));
    Diana_AesKeyGenAssistValue(&dest, &src, (int)imm);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &dest));
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// pclmulqdq: the carry-less product of the qword imm8[0] of dest and the qword imm8[4] of src

void Diana_PclmulValue(DianaRegisterXMM_type * pDest, const DianaRegisterXMM_type * pA,
                       const DianaRegisterXMM_type * pB, int imm)
{
    OPERAND_SIZE a = pA->u64[imm & 1], b = pB->u64[(imm >> 4) & 1], low = 0, high = 0;
    int i;
    for (i = 0; i < 64; ++i)
    {
        if (!((b >> i) & 1))
            continue;
        low ^= a << i;
        if (i)
            high ^= a >> (64 - i);
    }
    pDest->u64[0] = low;
    pDest->u64[1] = high;
}

int Diana_Call_pclmulqdq(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    DI_CHECK(Di_CryptoImm(pDianaContext, pCallContext, 2, &imm));
    Diana_PclmulValue(&dest, &dest, &src, (int)imm);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &dest));
    DI_PROC_END
}

// ---------------------------------------------------------------------------------------------------------------
// SHA

static DI_UINT32 Di_Rol(DI_UINT32 value, int count)
{
    return (value << count) | (value >> (32 - count));
}

static DI_UINT32 Di_Ror(DI_UINT32 value, int count)
{
    return (value >> count) | (value << (32 - count));
}

// sha1rnds4 xmm1, xmm2, imm8: four rounds, the function and the constant of imm8[1:0]; A B C D in dwords 3..0 of xmm1,
// W0+E W1 W2 W3 in dwords 3..0 of xmm2
int Diana_Call_sha1rnds4(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    static const DI_UINT32 constants[4] = {0x5A827999UL, 0x6ED9EBA1UL, 0x8F1BBCDCUL, 0xCA62C1D6UL};
    DianaRegisterXMM_type dest, src;
    OPERAND_SIZE imm = 0;
    DI_UINT32 a, b, c, d, e = 0, w[4], f, k, next;
    int i, function;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    DI_CHECK(Di_CryptoImm(pDianaContext, pCallContext, 2, &imm));
    function = (int)(imm & 3);
    k = constants[function];
    a = dest.u32[3];
    b = dest.u32[2];
    c = dest.u32[1];
    d = dest.u32[0];
    w[0] = src.u32[3];
    w[1] = src.u32[2];
    w[2] = src.u32[1];
    w[3] = src.u32[0];
    for (i = 0; i < 4; ++i)
    {
        switch (function)
        {
        case 0: f = (b & c) ^ (~b & d); break;
        case 2: f = (b & c) ^ (b & d) ^ (c & d); break;
        default: f = b ^ c ^ d; break;
        }
        // round 0: W0 already holds E
        next = f + Di_Rol(a, 5) + w[i] + k + (i ? e : 0);
        e = d;
        d = c;
        c = Di_Rol(b, 30);
        b = a;
        a = next;
    }
    dest.u32[3] = a;
    dest.u32[2] = b;
    dest.u32[1] = c;
    dest.u32[0] = d;
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &dest));
    DI_PROC_END
}

// sha1nexte: dword 3 of xmm2 + (dword 3 of xmm1 ROL 30), dwords 2..0 of xmm2
int Diana_Call_sha1nexte(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    src.u32[3] += Di_Rol(dest.u32[3], 30);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &src));
    DI_PROC_END
}

// sha1msg1: W0..W3 = dwords 3..0 of xmm1, W4 W5 = dwords 3, 2 of xmm2; W2^W0, W3^W1, W4^W2, W5^W3 to dwords 3..0
int Diana_Call_sha1msg1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    result.u32[3] = dest.u32[1] ^ dest.u32[3];
    result.u32[2] = dest.u32[0] ^ dest.u32[2];
    result.u32[1] = src.u32[3] ^ dest.u32[1];
    result.u32[0] = src.u32[2] ^ dest.u32[0];
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &result));
    DI_PROC_END
}

// sha1msg2: W13 W14 W15 = dwords 2..0 of xmm2; W16 = (dword 3 of xmm1 ^ W13) ROL 1, W17, W18 alike, W19 =
// (dword 0 of xmm1 ^ W16) ROL 1; W16..W19 to dwords 3..0
int Diana_Call_sha1msg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    DI_UINT32 w16, w17, w18, w19;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    w16 = Di_Rol(dest.u32[3] ^ src.u32[2], 1);
    w17 = Di_Rol(dest.u32[2] ^ src.u32[1], 1);
    w18 = Di_Rol(dest.u32[1] ^ src.u32[0], 1);
    w19 = Di_Rol(dest.u32[0] ^ w16, 1);
    result.u32[3] = w16;
    result.u32[2] = w17;
    result.u32[1] = w18;
    result.u32[0] = w19;
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &result));
    DI_PROC_END
}

static DI_UINT32 Di_Sigma0(DI_UINT32 w)
{
    return Di_Ror(w, 7) ^ Di_Ror(w, 18) ^ (w >> 3);
}

static DI_UINT32 Di_Sigma1(DI_UINT32 w)
{
    return Di_Ror(w, 17) ^ Di_Ror(w, 19) ^ (w >> 10);
}

// sha256msg1: W0..W3 = dwords 0..3 of xmm1, W4 = dword 0 of xmm2; Wi + sigma0(Wi+1) to dword i
int Diana_Call_sha256msg1(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    result.u32[3] = dest.u32[3] + Di_Sigma0(src.u32[0]);
    result.u32[2] = dest.u32[2] + Di_Sigma0(dest.u32[3]);
    result.u32[1] = dest.u32[1] + Di_Sigma0(dest.u32[2]);
    result.u32[0] = dest.u32[0] + Di_Sigma0(dest.u32[1]);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &result));
    DI_PROC_END
}

// sha256msg2: W14 W15 = dwords 2, 3 of xmm2; W16 = dword 0 of xmm1 + sigma1(W14), W17 = dword 1 + sigma1(W15),
// W18 = dword 2 + sigma1(W16), W19 = dword 3 + sigma1(W17) to dwords 0..3
int Diana_Call_sha256msg2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, result;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    result.u32[0] = dest.u32[0] + Di_Sigma1(src.u32[2]);
    result.u32[1] = dest.u32[1] + Di_Sigma1(src.u32[3]);
    result.u32[2] = dest.u32[2] + Di_Sigma1(result.u32[0]);
    result.u32[3] = dest.u32[3] + Di_Sigma1(result.u32[1]);
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &result));
    DI_PROC_END
}

// sha256rnds2 xmm1, xmm2/m128, <xmm0>: two rounds; A B E F of the state in xmm2, C D G H in xmm1 (dwords 3 2 1 0),
// the message plus the constant of the rounds in dwords 0, 1 of xmm0; A B E F of the result to xmm1
static DI_UINT32 Di_Sha256Sum0(DI_UINT32 a)
{
    return Di_Ror(a, 2) ^ Di_Ror(a, 13) ^ Di_Ror(a, 22);
}

static DI_UINT32 Di_Sha256Sum1(DI_UINT32 e)
{
    return Di_Ror(e, 6) ^ Di_Ror(e, 11) ^ Di_Ror(e, 25);
}

int Diana_Call_sha256rnds2(struct _dianaContext * pDianaContext, DianaProcessor * pCallContext)
{
    DianaRegisterXMM_type dest, src, wk, result;
    DI_UINT32 a, b, c, d, e, f, g, h;
    int i;
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 0, &dest));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 1, &src));
    DI_CHECK(Di_CryptoGet(pDianaContext, pCallContext, 2, &wk));
    a = src.u32[3]; b = src.u32[2]; c = dest.u32[3]; d = dest.u32[2];
    e = src.u32[1]; f = src.u32[0]; g = dest.u32[1]; h = dest.u32[0];
    for (i = 0; i < 2; ++i)
    {
        DI_UINT32 t = ((e & f) ^ (~e & g)) + Di_Sha256Sum1(e) + wk.u32[i] + h;
        DI_UINT32 nextA = t + ((a & b) ^ (a & c) ^ (b & c)) + Di_Sha256Sum0(a);
        DI_UINT32 nextE = t + d;
        h = g; g = f; f = e; e = nextE;
        d = c; c = b; b = a; a = nextA;
    }
    result.u32[3] = a;
    result.u32[2] = b;
    result.u32[1] = e;
    result.u32[0] = f;
    DI_CHECK(Di_CryptoSet(pDianaContext, pCallContext, 0, &result));
    DI_PROC_END
}
