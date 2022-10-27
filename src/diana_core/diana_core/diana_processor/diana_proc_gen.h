
#define GET_REG_AL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_AL)))
#define SET_REG_AL(Y) DianaProcessor_SetValue(pCallContext, reg_AL, DianaProcessor_QueryReg(pCallContext, reg_AL), Y)
#define GET_REG_CL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CL)))
#define SET_REG_CL(Y) DianaProcessor_SetValue(pCallContext, reg_CL, DianaProcessor_QueryReg(pCallContext, reg_CL), Y)
#define GET_REG_DL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DL)))
#define SET_REG_DL(Y) DianaProcessor_SetValue(pCallContext, reg_DL, DianaProcessor_QueryReg(pCallContext, reg_DL), Y)
#define GET_REG_BL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_BL)))
#define SET_REG_BL(Y) DianaProcessor_SetValue(pCallContext, reg_BL, DianaProcessor_QueryReg(pCallContext, reg_BL), Y)
#define GET_REG_AH ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_AH)))
#define SET_REG_AH(Y) DianaProcessor_SetValue(pCallContext, reg_AH, DianaProcessor_QueryReg(pCallContext, reg_AH), Y)
#define GET_REG_CH ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CH)))
#define SET_REG_CH(Y) DianaProcessor_SetValue(pCallContext, reg_CH, DianaProcessor_QueryReg(pCallContext, reg_CH), Y)
#define GET_REG_DH ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DH)))
#define SET_REG_DH(Y) DianaProcessor_SetValue(pCallContext, reg_DH, DianaProcessor_QueryReg(pCallContext, reg_DH), Y)
#define GET_REG_BH ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_BH)))
#define SET_REG_BH(Y) DianaProcessor_SetValue(pCallContext, reg_BH, DianaProcessor_QueryReg(pCallContext, reg_BH), Y)
#define GET_REG_AX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_AX)))
#define SET_REG_AX(Y) DianaProcessor_SetValue(pCallContext, reg_AX, DianaProcessor_QueryReg(pCallContext, reg_AX), Y)
#define GET_REG_CX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CX)))
#define SET_REG_CX(Y) DianaProcessor_SetValue(pCallContext, reg_CX, DianaProcessor_QueryReg(pCallContext, reg_CX), Y)
#define GET_REG_DX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DX)))
#define SET_REG_DX(Y) DianaProcessor_SetValue(pCallContext, reg_DX, DianaProcessor_QueryReg(pCallContext, reg_DX), Y)
#define GET_REG_BX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_BX)))
#define SET_REG_BX(Y) DianaProcessor_SetValue(pCallContext, reg_BX, DianaProcessor_QueryReg(pCallContext, reg_BX), Y)
#define GET_REG_SP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_SP)))
#define SET_REG_SP(Y) DianaProcessor_SetValue(pCallContext, reg_SP, DianaProcessor_QueryReg(pCallContext, reg_SP), Y)
#define GET_REG_BP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_BP)))
#define SET_REG_BP(Y) DianaProcessor_SetValue(pCallContext, reg_BP, DianaProcessor_QueryReg(pCallContext, reg_BP), Y)
#define GET_REG_SI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_SI)))
#define SET_REG_SI(Y) DianaProcessor_SetValue(pCallContext, reg_SI, DianaProcessor_QueryReg(pCallContext, reg_SI), Y)
#define GET_REG_DI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DI)))
#define SET_REG_DI(Y) DianaProcessor_SetValue(pCallContext, reg_DI, DianaProcessor_QueryReg(pCallContext, reg_DI), Y)
#define GET_REG_EAX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_EAX)))
#define SET_REG_EAX(Y) DianaProcessor_SetValue(pCallContext, reg_EAX, DianaProcessor_QueryReg(pCallContext, reg_EAX), Y)
#define GET_REG_ECX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_ECX)))
#define SET_REG_ECX(Y) DianaProcessor_SetValue(pCallContext, reg_ECX, DianaProcessor_QueryReg(pCallContext, reg_ECX), Y)
#define GET_REG_EDX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_EDX)))
#define SET_REG_EDX(Y) DianaProcessor_SetValue(pCallContext, reg_EDX, DianaProcessor_QueryReg(pCallContext, reg_EDX), Y)
#define GET_REG_EBX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_EBX)))
#define SET_REG_EBX(Y) DianaProcessor_SetValue(pCallContext, reg_EBX, DianaProcessor_QueryReg(pCallContext, reg_EBX), Y)
#define GET_REG_ESP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_ESP)))
#define SET_REG_ESP(Y) DianaProcessor_SetValue(pCallContext, reg_ESP, DianaProcessor_QueryReg(pCallContext, reg_ESP), Y)
#define GET_REG_EBP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_EBP)))
#define SET_REG_EBP(Y) DianaProcessor_SetValue(pCallContext, reg_EBP, DianaProcessor_QueryReg(pCallContext, reg_EBP), Y)
#define GET_REG_ESI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_ESI)))
#define SET_REG_ESI(Y) DianaProcessor_SetValue(pCallContext, reg_ESI, DianaProcessor_QueryReg(pCallContext, reg_ESI), Y)
#define GET_REG_EDI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_EDI)))
#define SET_REG_EDI(Y) DianaProcessor_SetValue(pCallContext, reg_EDI, DianaProcessor_QueryReg(pCallContext, reg_EDI), Y)
#define GET_REG_ES ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_ES)))
#define SET_REG_ES(Y) DianaProcessor_SetValue(pCallContext, reg_ES, DianaProcessor_QueryReg(pCallContext, reg_ES), Y)
#define GET_REG_CS ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CS)))
#define SET_REG_CS(Y) DianaProcessor_SetValue(pCallContext, reg_CS, DianaProcessor_QueryReg(pCallContext, reg_CS), Y)
#define GET_REG_SS ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_SS)))
#define SET_REG_SS(Y) DianaProcessor_SetValue(pCallContext, reg_SS, DianaProcessor_QueryReg(pCallContext, reg_SS), Y)
#define GET_REG_DS ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DS)))
#define SET_REG_DS(Y) DianaProcessor_SetValue(pCallContext, reg_DS, DianaProcessor_QueryReg(pCallContext, reg_DS), Y)
#define GET_REG_FS ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_FS)))
#define SET_REG_FS(Y) DianaProcessor_SetValue(pCallContext, reg_FS, DianaProcessor_QueryReg(pCallContext, reg_FS), Y)
#define GET_REG_GS ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_GS)))
#define SET_REG_GS(Y) DianaProcessor_SetValue(pCallContext, reg_GS, DianaProcessor_QueryReg(pCallContext, reg_GS), Y)
#define GET_REG_CR0 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR0)))
#define SET_REG_CR0(Y) DianaProcessor_SetValue(pCallContext, reg_CR0, DianaProcessor_QueryReg(pCallContext, reg_CR0), Y)
#define GET_REG_CR1 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR1)))
#define SET_REG_CR1(Y) DianaProcessor_SetValue(pCallContext, reg_CR1, DianaProcessor_QueryReg(pCallContext, reg_CR1), Y)
#define GET_REG_CR2 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR2)))
#define SET_REG_CR2(Y) DianaProcessor_SetValue(pCallContext, reg_CR2, DianaProcessor_QueryReg(pCallContext, reg_CR2), Y)
#define GET_REG_CR3 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR3)))
#define SET_REG_CR3(Y) DianaProcessor_SetValue(pCallContext, reg_CR3, DianaProcessor_QueryReg(pCallContext, reg_CR3), Y)
#define GET_REG_CR4 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR4)))
#define SET_REG_CR4(Y) DianaProcessor_SetValue(pCallContext, reg_CR4, DianaProcessor_QueryReg(pCallContext, reg_CR4), Y)
#define GET_REG_CR5 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR5)))
#define SET_REG_CR5(Y) DianaProcessor_SetValue(pCallContext, reg_CR5, DianaProcessor_QueryReg(pCallContext, reg_CR5), Y)
#define GET_REG_CR6 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR6)))
#define SET_REG_CR6(Y) DianaProcessor_SetValue(pCallContext, reg_CR6, DianaProcessor_QueryReg(pCallContext, reg_CR6), Y)
#define GET_REG_CR7 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_CR7)))
#define SET_REG_CR7(Y) DianaProcessor_SetValue(pCallContext, reg_CR7, DianaProcessor_QueryReg(pCallContext, reg_CR7), Y)
#define GET_REG_DR0 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR0)))
#define SET_REG_DR0(Y) DianaProcessor_SetValue(pCallContext, reg_DR0, DianaProcessor_QueryReg(pCallContext, reg_DR0), Y)
#define GET_REG_DR1 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR1)))
#define SET_REG_DR1(Y) DianaProcessor_SetValue(pCallContext, reg_DR1, DianaProcessor_QueryReg(pCallContext, reg_DR1), Y)
#define GET_REG_DR2 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR2)))
#define SET_REG_DR2(Y) DianaProcessor_SetValue(pCallContext, reg_DR2, DianaProcessor_QueryReg(pCallContext, reg_DR2), Y)
#define GET_REG_DR3 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR3)))
#define SET_REG_DR3(Y) DianaProcessor_SetValue(pCallContext, reg_DR3, DianaProcessor_QueryReg(pCallContext, reg_DR3), Y)
#define GET_REG_DR4 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR4)))
#define SET_REG_DR4(Y) DianaProcessor_SetValue(pCallContext, reg_DR4, DianaProcessor_QueryReg(pCallContext, reg_DR4), Y)
#define GET_REG_DR5 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR5)))
#define SET_REG_DR5(Y) DianaProcessor_SetValue(pCallContext, reg_DR5, DianaProcessor_QueryReg(pCallContext, reg_DR5), Y)
#define GET_REG_DR6 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR6)))
#define SET_REG_DR6(Y) DianaProcessor_SetValue(pCallContext, reg_DR6, DianaProcessor_QueryReg(pCallContext, reg_DR6), Y)
#define GET_REG_DR7 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DR7)))
#define SET_REG_DR7(Y) DianaProcessor_SetValue(pCallContext, reg_DR7, DianaProcessor_QueryReg(pCallContext, reg_DR7), Y)
#define GET_REG_TR0 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR0)))
#define SET_REG_TR0(Y) DianaProcessor_SetValue(pCallContext, reg_TR0, DianaProcessor_QueryReg(pCallContext, reg_TR0), Y)
#define GET_REG_TR1 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR1)))
#define SET_REG_TR1(Y) DianaProcessor_SetValue(pCallContext, reg_TR1, DianaProcessor_QueryReg(pCallContext, reg_TR1), Y)
#define GET_REG_TR2 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR2)))
#define SET_REG_TR2(Y) DianaProcessor_SetValue(pCallContext, reg_TR2, DianaProcessor_QueryReg(pCallContext, reg_TR2), Y)
#define GET_REG_TR3 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR3)))
#define SET_REG_TR3(Y) DianaProcessor_SetValue(pCallContext, reg_TR3, DianaProcessor_QueryReg(pCallContext, reg_TR3), Y)
#define GET_REG_TR4 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR4)))
#define SET_REG_TR4(Y) DianaProcessor_SetValue(pCallContext, reg_TR4, DianaProcessor_QueryReg(pCallContext, reg_TR4), Y)
#define GET_REG_TR5 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR5)))
#define SET_REG_TR5(Y) DianaProcessor_SetValue(pCallContext, reg_TR5, DianaProcessor_QueryReg(pCallContext, reg_TR5), Y)
#define GET_REG_TR6 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR6)))
#define SET_REG_TR6(Y) DianaProcessor_SetValue(pCallContext, reg_TR6, DianaProcessor_QueryReg(pCallContext, reg_TR6), Y)
#define GET_REG_TR7 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_TR7)))
#define SET_REG_TR7(Y) DianaProcessor_SetValue(pCallContext, reg_TR7, DianaProcessor_QueryReg(pCallContext, reg_TR7), Y)
#define GET_REG_RAX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RAX)))
#define SET_REG_RAX(Y) DianaProcessor_SetValue(pCallContext, reg_RAX, DianaProcessor_QueryReg(pCallContext, reg_RAX), Y)
#define GET_REG_RCX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RCX)))
#define SET_REG_RCX(Y) DianaProcessor_SetValue(pCallContext, reg_RCX, DianaProcessor_QueryReg(pCallContext, reg_RCX), Y)
#define GET_REG_RDX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RDX)))
#define SET_REG_RDX(Y) DianaProcessor_SetValue(pCallContext, reg_RDX, DianaProcessor_QueryReg(pCallContext, reg_RDX), Y)
#define GET_REG_RBX ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RBX)))
#define SET_REG_RBX(Y) DianaProcessor_SetValue(pCallContext, reg_RBX, DianaProcessor_QueryReg(pCallContext, reg_RBX), Y)
#define GET_REG_RSP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RSP)))
#define SET_REG_RSP(Y) DianaProcessor_SetValue(pCallContext, reg_RSP, DianaProcessor_QueryReg(pCallContext, reg_RSP), Y)
#define GET_REG_RBP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RBP)))
#define SET_REG_RBP(Y) DianaProcessor_SetValue(pCallContext, reg_RBP, DianaProcessor_QueryReg(pCallContext, reg_RBP), Y)
#define GET_REG_RSI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RSI)))
#define SET_REG_RSI(Y) DianaProcessor_SetValue(pCallContext, reg_RSI, DianaProcessor_QueryReg(pCallContext, reg_RSI), Y)
#define GET_REG_RDI ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RDI)))
#define SET_REG_RDI(Y) DianaProcessor_SetValue(pCallContext, reg_RDI, DianaProcessor_QueryReg(pCallContext, reg_RDI), Y)
#define GET_REG_SIL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_SIL)))
#define SET_REG_SIL(Y) DianaProcessor_SetValue(pCallContext, reg_SIL, DianaProcessor_QueryReg(pCallContext, reg_SIL), Y)
#define GET_REG_DIL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_DIL)))
#define SET_REG_DIL(Y) DianaProcessor_SetValue(pCallContext, reg_DIL, DianaProcessor_QueryReg(pCallContext, reg_DIL), Y)
#define GET_REG_BPL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_BPL)))
#define SET_REG_BPL(Y) DianaProcessor_SetValue(pCallContext, reg_BPL, DianaProcessor_QueryReg(pCallContext, reg_BPL), Y)
#define GET_REG_SPL ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_SPL)))
#define SET_REG_SPL(Y) DianaProcessor_SetValue(pCallContext, reg_SPL, DianaProcessor_QueryReg(pCallContext, reg_SPL), Y)
#define GET_REG_R8 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R8)))
#define SET_REG_R8(Y) DianaProcessor_SetValue(pCallContext, reg_R8, DianaProcessor_QueryReg(pCallContext, reg_R8), Y)
#define GET_REG_R9 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R9)))
#define SET_REG_R9(Y) DianaProcessor_SetValue(pCallContext, reg_R9, DianaProcessor_QueryReg(pCallContext, reg_R9), Y)
#define GET_REG_R10 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R10)))
#define SET_REG_R10(Y) DianaProcessor_SetValue(pCallContext, reg_R10, DianaProcessor_QueryReg(pCallContext, reg_R10), Y)
#define GET_REG_R11 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R11)))
#define SET_REG_R11(Y) DianaProcessor_SetValue(pCallContext, reg_R11, DianaProcessor_QueryReg(pCallContext, reg_R11), Y)
#define GET_REG_R12 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R12)))
#define SET_REG_R12(Y) DianaProcessor_SetValue(pCallContext, reg_R12, DianaProcessor_QueryReg(pCallContext, reg_R12), Y)
#define GET_REG_R13 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R13)))
#define SET_REG_R13(Y) DianaProcessor_SetValue(pCallContext, reg_R13, DianaProcessor_QueryReg(pCallContext, reg_R13), Y)
#define GET_REG_R14 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R14)))
#define SET_REG_R14(Y) DianaProcessor_SetValue(pCallContext, reg_R14, DianaProcessor_QueryReg(pCallContext, reg_R14), Y)
#define GET_REG_R15 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R15)))
#define SET_REG_R15(Y) DianaProcessor_SetValue(pCallContext, reg_R15, DianaProcessor_QueryReg(pCallContext, reg_R15), Y)
#define GET_REG_R8D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R8D)))
#define SET_REG_R8D(Y) DianaProcessor_SetValue(pCallContext, reg_R8D, DianaProcessor_QueryReg(pCallContext, reg_R8D), Y)
#define GET_REG_R9D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R9D)))
#define SET_REG_R9D(Y) DianaProcessor_SetValue(pCallContext, reg_R9D, DianaProcessor_QueryReg(pCallContext, reg_R9D), Y)
#define GET_REG_R10D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R10D)))
#define SET_REG_R10D(Y) DianaProcessor_SetValue(pCallContext, reg_R10D, DianaProcessor_QueryReg(pCallContext, reg_R10D), Y)
#define GET_REG_R11D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R11D)))
#define SET_REG_R11D(Y) DianaProcessor_SetValue(pCallContext, reg_R11D, DianaProcessor_QueryReg(pCallContext, reg_R11D), Y)
#define GET_REG_R12D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R12D)))
#define SET_REG_R12D(Y) DianaProcessor_SetValue(pCallContext, reg_R12D, DianaProcessor_QueryReg(pCallContext, reg_R12D), Y)
#define GET_REG_R13D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R13D)))
#define SET_REG_R13D(Y) DianaProcessor_SetValue(pCallContext, reg_R13D, DianaProcessor_QueryReg(pCallContext, reg_R13D), Y)
#define GET_REG_R14D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R14D)))
#define SET_REG_R14D(Y) DianaProcessor_SetValue(pCallContext, reg_R14D, DianaProcessor_QueryReg(pCallContext, reg_R14D), Y)
#define GET_REG_R15D ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R15D)))
#define SET_REG_R15D(Y) DianaProcessor_SetValue(pCallContext, reg_R15D, DianaProcessor_QueryReg(pCallContext, reg_R15D), Y)
#define GET_REG_R8W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R8W)))
#define SET_REG_R8W(Y) DianaProcessor_SetValue(pCallContext, reg_R8W, DianaProcessor_QueryReg(pCallContext, reg_R8W), Y)
#define GET_REG_R9W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R9W)))
#define SET_REG_R9W(Y) DianaProcessor_SetValue(pCallContext, reg_R9W, DianaProcessor_QueryReg(pCallContext, reg_R9W), Y)
#define GET_REG_R10W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R10W)))
#define SET_REG_R10W(Y) DianaProcessor_SetValue(pCallContext, reg_R10W, DianaProcessor_QueryReg(pCallContext, reg_R10W), Y)
#define GET_REG_R11W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R11W)))
#define SET_REG_R11W(Y) DianaProcessor_SetValue(pCallContext, reg_R11W, DianaProcessor_QueryReg(pCallContext, reg_R11W), Y)
#define GET_REG_R12W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R12W)))
#define SET_REG_R12W(Y) DianaProcessor_SetValue(pCallContext, reg_R12W, DianaProcessor_QueryReg(pCallContext, reg_R12W), Y)
#define GET_REG_R13W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R13W)))
#define SET_REG_R13W(Y) DianaProcessor_SetValue(pCallContext, reg_R13W, DianaProcessor_QueryReg(pCallContext, reg_R13W), Y)
#define GET_REG_R14W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R14W)))
#define SET_REG_R14W(Y) DianaProcessor_SetValue(pCallContext, reg_R14W, DianaProcessor_QueryReg(pCallContext, reg_R14W), Y)
#define GET_REG_R15W ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R15W)))
#define SET_REG_R15W(Y) DianaProcessor_SetValue(pCallContext, reg_R15W, DianaProcessor_QueryReg(pCallContext, reg_R15W), Y)
#define GET_REG_R8B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R8B)))
#define SET_REG_R8B(Y) DianaProcessor_SetValue(pCallContext, reg_R8B, DianaProcessor_QueryReg(pCallContext, reg_R8B), Y)
#define GET_REG_R9B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R9B)))
#define SET_REG_R9B(Y) DianaProcessor_SetValue(pCallContext, reg_R9B, DianaProcessor_QueryReg(pCallContext, reg_R9B), Y)
#define GET_REG_R10B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R10B)))
#define SET_REG_R10B(Y) DianaProcessor_SetValue(pCallContext, reg_R10B, DianaProcessor_QueryReg(pCallContext, reg_R10B), Y)
#define GET_REG_R11B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R11B)))
#define SET_REG_R11B(Y) DianaProcessor_SetValue(pCallContext, reg_R11B, DianaProcessor_QueryReg(pCallContext, reg_R11B), Y)
#define GET_REG_R12B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R12B)))
#define SET_REG_R12B(Y) DianaProcessor_SetValue(pCallContext, reg_R12B, DianaProcessor_QueryReg(pCallContext, reg_R12B), Y)
#define GET_REG_R13B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R13B)))
#define SET_REG_R13B(Y) DianaProcessor_SetValue(pCallContext, reg_R13B, DianaProcessor_QueryReg(pCallContext, reg_R13B), Y)
#define GET_REG_R14B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R14B)))
#define SET_REG_R14B(Y) DianaProcessor_SetValue(pCallContext, reg_R14B, DianaProcessor_QueryReg(pCallContext, reg_R14B), Y)
#define GET_REG_R15B ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_R15B)))
#define SET_REG_R15B(Y) DianaProcessor_SetValue(pCallContext, reg_R15B, DianaProcessor_QueryReg(pCallContext, reg_R15B), Y)

#define GET_REG_IP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_IP)))
#define SET_REG_IP(Y) DianaProcessor_SetValue(pCallContext, reg_IP, DianaProcessor_QueryReg(pCallContext, reg_IP), Y)
#define GET_REG_RIP ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RIP)))
#define SET_REG_RIP(Y) DianaProcessor_SetValue(pCallContext, reg_RIP, DianaProcessor_QueryReg(pCallContext, reg_RIP), Y)

#define GET_REG_MM0 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM0)))
#define SET_REG_MM0(Y) DianaProcessor_SetValue(pCallContext, reg_MM0, DianaProcessor_QueryReg(pCallContext, reg_MM0), Y)
#define GET_REG_MM1 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM1)))
#define SET_REG_MM1(Y) DianaProcessor_SetValue(pCallContext, reg_MM1, DianaProcessor_QueryReg(pCallContext, reg_MM1), Y)
#define GET_REG_MM2 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM2)))
#define SET_REG_MM2(Y) DianaProcessor_SetValue(pCallContext, reg_MM2, DianaProcessor_QueryReg(pCallContext, reg_MM2), Y)
#define GET_REG_MM3 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM3)))
#define SET_REG_MM3(Y) DianaProcessor_SetValue(pCallContext, reg_MM3, DianaProcessor_QueryReg(pCallContext, reg_MM3), Y)
#define GET_REG_MM4 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM4)))
#define SET_REG_MM4(Y) DianaProcessor_SetValue(pCallContext, reg_MM4, DianaProcessor_QueryReg(pCallContext, reg_MM4), Y)
#define GET_REG_MM5 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM5)))
#define SET_REG_MM5(Y) DianaProcessor_SetValue(pCallContext, reg_MM5, DianaProcessor_QueryReg(pCallContext, reg_MM5), Y)
#define GET_REG_MM6 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM6)))
#define SET_REG_MM6(Y) DianaProcessor_SetValue(pCallContext, reg_MM6, DianaProcessor_QueryReg(pCallContext, reg_MM6), Y)
#define GET_REG_MM7 ((OPERAND_SIZE)DianaProcessor_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_MM7)))
#define SET_REG_MM7(Y) DianaProcessor_SetValue(pCallContext, reg_MM7, DianaProcessor_QueryReg(pCallContext, reg_MM7), Y)

// FPU
#define GET_REG_FPU_ST0     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST0)
#define SET_REG_FPU_ST0(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST0, (Y))
#define GET_REG_FPU_ST1     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST1)
#define SET_REG_FPU_ST1(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST1, (Y))
#define GET_REG_FPU_ST2     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST2)
#define SET_REG_FPU_ST2(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST2, (Y))
#define GET_REG_FPU_ST3     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST3)
#define SET_REG_FPU_ST3(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST3, (Y))
#define GET_REG_FPU_ST4     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST4)
#define SET_REG_FPU_ST4(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST4, (Y))
#define GET_REG_FPU_ST5     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST5)
#define SET_REG_FPU_ST5(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST5, (Y))
#define GET_REG_FPU_ST6     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST6)
#define SET_REG_FPU_ST6(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST6, (Y))
#define GET_REG_FPU_ST7     (OPERAND_SIZE)DianaProcessor_FPU_GetSTRegister(pCallContext, reg_fpu_ST7)
#define SET_REG_FPU_ST7(Y)  DianaProcessor_FPU_SetSTRegister(pCallContext, reg_fpu_ST7, (Y))

#define GET_FLAG_CF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_CF ))
#define SET_FLAG_CF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_CF ))
#define CLEAR_FLAG_CF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_CF ))
#define GET_FLAG_PF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_PF ))
#define SET_FLAG_PF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_PF ))
#define CLEAR_FLAG_PF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_PF ))
#define GET_FLAG_AF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_AF ))
#define SET_FLAG_AF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_AF ))
#define CLEAR_FLAG_AF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_AF ))
#define GET_FLAG_ZF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_ZF ))
#define SET_FLAG_ZF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_ZF ))
#define CLEAR_FLAG_ZF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_ZF ))
#define GET_FLAG_SF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_SF ))
#define SET_FLAG_SF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_SF ))
#define CLEAR_FLAG_SF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_SF ))
#define GET_FLAG_TF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_TF ))
#define SET_FLAG_TF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_TF ))
#define CLEAR_FLAG_TF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_TF ))
#define GET_FLAG_IF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_IF ))
#define SET_FLAG_IF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_IF ))
#define CLEAR_FLAG_IF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_IF ))
#define GET_FLAG_DF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_DF ))
#define SET_FLAG_DF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_DF ))
#define CLEAR_FLAG_DF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_DF ))
#define GET_FLAG_OF (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_OF ))
#define SET_FLAG_OF (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_OF ))
#define CLEAR_FLAG_OF (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_OF ))

#define GET_FLAG_ID (DianaProcessor_QueryFlag(pCallContext, DI_FLAG_ID ))
#define SET_FLAG_ID (DianaProcessor_SetFlag(pCallContext,  DI_FLAG_ID ))
#define CLEAR_FLAG_ID (DianaProcessor_ClearFlag(pCallContext,  DI_FLAG_ID ))

#define SET_REG_RBP2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RBP, DianaProcessor_QueryReg(pCallContext, reg_RBP), Y, Size)
#define GET_REG_RBP2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RBP), Size))

#define SET_REG_RSP2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RSP, DianaProcessor_QueryReg(pCallContext, reg_RSP), Y, Size)
#define GET_REG_RSP2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RSP), Size))

#define SET_REG_RIP2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RIP, DianaProcessor_QueryReg(pCallContext, reg_RIP), Y, Size)
#define GET_REG_RIP2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RIP), Size))

#define SET_REG_RSI2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RSI, DianaProcessor_QueryReg(pCallContext, reg_RSI), Y, Size)
#define GET_REG_RSI2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RSI), Size))

#define SET_REG_RAX2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RAX, DianaProcessor_QueryReg(pCallContext, reg_RAX), Y, Size)
#define GET_REG_RAX2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RAX), Size))

#define SET_REG_RBX2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RBX, DianaProcessor_QueryReg(pCallContext, reg_RBX), Y, Size)
#define GET_REG_RBX2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RBX), Size))

#define SET_REG_RCX2(Y, Size) DianaProcessor_SetValueEx(pCallContext, reg_RCX, DianaProcessor_QueryReg(pCallContext, reg_RCX), Y, Size)
#define GET_REG_RCX2(Size) ((OPERAND_SIZE)DianaProcessor_GetValueEx(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_RCX), Size))

#define DI_JUMP_TO_RIP(Y) SET_REG_RIP(Y); DianaProcessor_ClearCache(pCallContext);

#define SET_REG_XMM0(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM0, DianaProcessor_QueryReg(pCallContext, reg_XMM0), &Y)
#define GET_REG_XMM0 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM0))
#define SET_REG_XMM1(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM1, DianaProcessor_QueryReg(pCallContext, reg_XMM1), &Y)
#define GET_REG_XMM1 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM1))
#define SET_REG_XMM2(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM2, DianaProcessor_QueryReg(pCallContext, reg_XMM2), &Y)
#define GET_REG_XMM2 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM2))
#define SET_REG_XMM3(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM3, DianaProcessor_QueryReg(pCallContext, reg_XMM3), &Y)
#define GET_REG_XMM3 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM3))
#define SET_REG_XMM4(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM4, DianaProcessor_QueryReg(pCallContext, reg_XMM4), &Y)
#define GET_REG_XMM4 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM4))
#define SET_REG_XMM5(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM5, DianaProcessor_QueryReg(pCallContext, reg_XMM5), &Y)
#define GET_REG_XMM5 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM5))
#define SET_REG_XMM6(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM6, DianaProcessor_QueryReg(pCallContext, reg_XMM6), &Y)
#define GET_REG_XMM6 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM6))
#define SET_REG_XMM7(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM7, DianaProcessor_QueryReg(pCallContext, reg_XMM7), &Y)
#define GET_REG_XMM7 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM7))
#define SET_REG_XMM8(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM8, DianaProcessor_QueryReg(pCallContext, reg_XMM8), &Y)
#define GET_REG_XMM8 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM8))
#define SET_REG_XMM9(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM9, DianaProcessor_QueryReg(pCallContext, reg_XMM9), &Y)
#define GET_REG_XMM9 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM9))
#define SET_REG_XMM10(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM10, DianaProcessor_QueryReg(pCallContext, reg_XMM10), &Y)
#define GET_REG_XMM10 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM10))
#define SET_REG_XMM11(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM11, DianaProcessor_QueryReg(pCallContext, reg_XMM11), &Y)
#define GET_REG_XMM11 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM11))
#define SET_REG_XMM12(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM12, DianaProcessor_QueryReg(pCallContext, reg_XMM12), &Y)
#define GET_REG_XMM12 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM12))
#define SET_REG_XMM13(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM13, DianaProcessor_QueryReg(pCallContext, reg_XMM13), &Y)
#define GET_REG_XMM13 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM13))
#define SET_REG_XMM14(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM14, DianaProcessor_QueryReg(pCallContext, reg_XMM14), &Y)
#define GET_REG_XMM14 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM14))
#define SET_REG_XMM15(Y) DianaProcessor_XMM_SetValue(pCallContext, reg_XMM15, DianaProcessor_QueryReg(pCallContext, reg_XMM15), &Y)
#define GET_REG_XMM15 DianaProcessor_XMM_GetValue(pCallContext, DianaProcessor_QueryReg(pCallContext, reg_XMM15))
