#ifndef DIANA_PROCESSOR_CONTEXT_H
#define DIANA_PROCESSOR_CONTEXT_H

#include "diana_processor_core_impl.h"

#define DIANA_PROCESSOR_CONTEXT_FPU_STATE_VALID     1 

typedef struct _Diana_Processor_Registers_Context
{
    DianaRegisterValue_type reg_RIP;
    DianaRegisterValue_type flags;
    DianaFPU  fpuState;
    int fpuStateFlags;

    DianaRegisterValue16_type reg_ES;
    DianaRegisterValue16_type reg_CS;
    DianaRegisterValue16_type reg_SS;
    DianaRegisterValue16_type reg_DS;
    DianaRegisterValue16_type reg_FS;
    DianaRegisterValue16_type reg_GS;

    DianaRegisterValue32_type reg_CR0;
    DianaRegisterValue32_type reg_CR1;
    DianaRegisterValue32_type reg_CR2;
    DianaRegisterValue32_type reg_CR3;
    DianaRegisterValue32_type reg_CR4;
    DianaRegisterValue32_type reg_CR5;
    DianaRegisterValue32_type reg_CR6;
    DianaRegisterValue32_type reg_CR7;

    DianaRegisterValue_type reg_DR0;
    DianaRegisterValue_type reg_DR1;
    DianaRegisterValue_type reg_DR2;
    DianaRegisterValue_type reg_DR3;
    DianaRegisterValue_type reg_DR4;
    DianaRegisterValue_type reg_DR5;
    DianaRegisterValue_type reg_DR6;
    DianaRegisterValue_type reg_DR7;

    DianaRegisterValue32_type reg_TR0;
    DianaRegisterValue32_type reg_TR1;
    DianaRegisterValue32_type reg_TR2;
    DianaRegisterValue32_type reg_TR3;
    DianaRegisterValue32_type reg_TR4;
    DianaRegisterValue32_type reg_TR5;
    DianaRegisterValue32_type reg_TR6;
    DianaRegisterValue32_type reg_TR7;

    DianaRegisterValue_type reg_RAX;
    DianaRegisterValue_type reg_RCX;
    DianaRegisterValue_type reg_RDX;
    DianaRegisterValue_type reg_RBX;
    DianaRegisterValue_type reg_RSP;
    DianaRegisterValue_type reg_RBP;
    DianaRegisterValue_type reg_RSI;
    DianaRegisterValue_type reg_RDI;
    DianaRegisterValue_type reg_R8;
    DianaRegisterValue_type reg_R9;
    DianaRegisterValue_type reg_R10;
    DianaRegisterValue_type reg_R11;
    DianaRegisterValue_type reg_R12;
    DianaRegisterValue_type reg_R13;
    DianaRegisterValue_type reg_R14;
    DianaRegisterValue_type reg_R15;


    DianaRegisterValue_type reg_MM0; // reg_FPU_ST0
    DianaRegisterValue_type reg_MM1; // reg_FPU_ST1
    DianaRegisterValue_type reg_MM2;
    DianaRegisterValue_type reg_MM3;
    DianaRegisterValue_type reg_MM4;
    DianaRegisterValue_type reg_MM5;
    DianaRegisterValue_type reg_MM6;
    DianaRegisterValue_type reg_MM7; // reg_FPU_ST7

    DianaRegisterValue128_type reg_XMM0;
    DianaRegisterValue128_type reg_XMM1;
    DianaRegisterValue128_type reg_XMM2;
    DianaRegisterValue128_type reg_XMM3;
    DianaRegisterValue128_type reg_XMM4;
    DianaRegisterValue128_type reg_XMM5;
    DianaRegisterValue128_type reg_XMM6;
    DianaRegisterValue128_type reg_XMM7;
    DianaRegisterValue128_type reg_XMM8;
    DianaRegisterValue128_type reg_XMM9;
    DianaRegisterValue128_type reg_XMM10;
    DianaRegisterValue128_type reg_XMM11;
    DianaRegisterValue128_type reg_XMM12;
    DianaRegisterValue128_type reg_XMM13;
    DianaRegisterValue128_type reg_XMM14;
    DianaRegisterValue128_type reg_XMM15;

}Diana_Processor_Registers_Context;


int DianaProcessor_InitContext(DianaProcessor * pProcessor, const Diana_Processor_Registers_Context * pContext);
int DianaProcessor_QueryContext(DianaProcessor * pProcessor, Diana_Processor_Registers_Context * pContext);


#endif