#include "string"
#include "cctype"
#include "iostream"
#include "algorithm"
#include "diana_processor_cpp.h"

const char * g_pData = 
"reg_AL,    reg_CL,    reg_DL,    reg_BL,    reg_AH,    reg_CH,    reg_DH,    reg_BH,"
"reg_AX,    reg_CX,    reg_DX,    reg_BX,    reg_SP,    reg_BP,    reg_SI,    reg_DI,"
"reg_EAX,   reg_ECX,   reg_EDX,   reg_EBX,   reg_ESP,   reg_EBP,   reg_ESI,   reg_EDI,"
"reg_ES,    reg_CS,    reg_SS,    reg_DS,    reg_FS,    reg_GS,"
"reg_CR0,   reg_CR1,   reg_CR2,   reg_CR3,   reg_CR4,   reg_CR5,   reg_CR6,   reg_CR7,"
"reg_DR0,   reg_DR1,   reg_DR2,   reg_DR3,   reg_DR4,   reg_DR5,   reg_DR6,   reg_DR7,"
"reg_TR0,   reg_TR1,   reg_TR2,   reg_TR3,   reg_TR4,   reg_TR5,   reg_TR6,   reg_TR7,"
"reg_IP,"
"reg_RAX,   reg_RCX,   reg_RDX,   reg_RBX,   reg_RSP,   reg_RBP,   reg_RSI,   reg_RDI,"
"reg_SIL,   reg_DIL,   reg_BPL,   reg_SPL,"
"reg_R8,    reg_R9,    reg_R10,   reg_R11,   reg_R12,   reg_R13,   reg_R14,   reg_R15,"
"reg_R8D,   reg_R9D,   reg_R10D,  reg_R11D,  reg_R12D,  reg_R13D,  reg_R14D,  reg_R15D,"
"reg_R8W,   reg_R9W,   reg_R10W,  reg_R11W,  reg_R12W,  reg_R13W,  reg_R14W,  reg_R15W,"
"reg_R8B,   reg_R9B,   reg_R10B,  reg_R11B,  reg_R12B,  reg_R13B,  reg_R14B,  reg_R15B,"
"reg_RIP,"
"reg_MM0, reg_MM1, reg_MM2, reg_MM3, reg_MM4, reg_MM5, reg_MM6, reg_MM7,"
"reg_MM8, reg_MM9, reg_MM10, reg_MM11, reg_MM12, reg_MM13, reg_MM14, reg_MM15,"
"reg_XMM0, reg_XMM1, reg_XMM2, reg_XMM3, reg_XMM4, reg_XMM5, reg_XMM6, reg_XMM7,"
"reg_XMM8, reg_XMM9, reg_XMM10, reg_XMM11, reg_XMM12, reg_XMM13, reg_XMM14, reg_XMM15,"
"reg_fpu_ST0, reg_fpu_ST1, reg_fpu_ST2, reg_fpu_ST3, reg_fpu_ST4, reg_fpu_ST5, reg_fpu_ST6, reg_fpu_ST7";


void di_gen()
{
    std::cout<<Diana_GenerateStuff()<<"\n\n";
    int i = 0;
    while(g_pData[i])
    {
        int iBegin = i;
        while(g_pData[i] && g_pData[i]!=' ' && g_pData[i]!=',')
            ++i;

        if (!g_pData[i])
            return;

        std::string word(g_pData + iBegin, g_pData+i);
        std::string upperWord(word);
    
        std::transform(upperWord.begin(), upperWord.end(), upperWord.begin(), std::toupper);

        std::cout<<"    DIANA_GENERATE_IMPL("<<upperWord<<", "<<word<<")\n";

        while(g_pData[i] && (g_pData[i]==' ' || g_pData[i]==','))
            ++i;
    }

}