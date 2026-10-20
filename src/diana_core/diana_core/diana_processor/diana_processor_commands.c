#include "diana_processor_commands.h"

#include "diana_processor_cmd_a.h"
#include "diana_processor_cmd_b.h"
#include "diana_processor_cmd_c.h"
#include "diana_processor_cmd_d.h"
#include "diana_processor_cmd_i.h"
#include "diana_processor_cmd_j.h"
#include "diana_processor_cmd_l.h"
#include "diana_processor_cmd_l_xmm.h"
#include "diana_processor_cmd_m.h"
#include "diana_processor_cmd_m_xmm.h"
#include "diana_processor_cmd_n.h"
#include "diana_processor_cmd_p.h"
#include "diana_processor_cmd_r.h"
#include "diana_processor_cmd_s.h"
#include "diana_processor_cmd_s2.h"
#include "diana_processor_cmd_fpu.h"

#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"

#include "diana_disable_warnings.h"
#include "diana_processor_cmd_fpu_sse.h"
#include "diana_processor_cmd_fpu2.h"
#include "diana_processor_cmd_sse2.h"
#include "diana_processor_cmd_ext.h"
#include "diana_processor_cmd_simd.h"
#include "diana_processor_cmd_crypto.h"
#include "diana_processor_cmd_avx.h"
#include "diana_processor_cmd_avx2.h"
#include "diana_processor_cmd_avx512.h"

int Diana_Call_xor(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := LeftSRC XOR RightSRC
    //CF := 0
    //OF := 0

    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));
    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;

    dest = dest ^ src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_xadd(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //temp := DEST
    //DEST := SRC
    //SRC := temp 
    //DEST := DEST + SRC
    OPERAND_SIZE temp = 0, addend = 0;
    DI_DEF_LOCALS(src, dest);

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    temp = dest;
    addend = src;
    DI_START_UPDATE_COA_FLAGS(dest);
    dest = dest + src;
    src = temp;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));//first operand
    DI_CHECK(Di_CheckZeroExtends2(pCallContext, &src, dest_size, &src_size));//second operand
    // OF: the sign of the addend (the old SRC), not of the old DEST
    DI_END_UPDATE_COA_FLAGS_ADD(dest, addend);
    DI_MEM_SET_SRC(src);
    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));//DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_xchg(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //temp := DEST
    //DEST := SRC
    //SRC := temp 

    OPERAND_SIZE temp = 0;
    int temp_size = 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    temp_size = dest_size;
    temp = dest;
    dest = src;
    src = temp;

    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));//first operand
    DI_CHECK(Di_CheckZeroExtends2(pCallContext, &src, dest_size, &src_size));//second operand
    DI_MEM_SET_DEST(dest);
    DI_MEM_SET_SRC(src);
    DI_PROC_END
}

static
int Diana_Call_test(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //DEST := DEST AND SRC;
    //CF := 0;
    //OF := 0;
    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    dest = dest & src;
    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;

    DianaProcessor_UpdatePSZ(pCallContext, 
                             dest, 
                             pCallContext->m_result.linkedOperands->usedSize);
    // UNDOCUMENTED ***************
    CLEAR_FLAG_AF;
    // ****************************

    DI_PROC_END
}

static
int Diana_Call_inc(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    //DEST := DEST + SRC;
    DI_DEF_LOCALS(src, dest);
    src_size;
        
    DI_MEM_GET_DEST(dest);

    src = 1;

    // update AF, OF
    DI_START_UPDATE_OA_FLAGS(dest);

    dest = dest + src;

    DI_END_UPDATE_OA_FLAGS_ADD(dest, src);

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));
    DI_PROC_END
}

int Diana_Call_ret(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    return Diana_Call_internal_ret(pDianaContext,
                                   pCallContext,
                                   0);
}
int Diana_Call_retf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    return Diana_Call_internal_ret(pDianaContext,
                                   pCallContext,
                                   1);
}


int Diana_Call_or(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //DEST := DEST OR SRC;
    //CF := 0;
    //OF := 0;

    DI_DEF_LOCALS(src, dest);
    oldDestValue;

    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    DI_SIGN_EXTEND(src, DI_VAR_SIZE(dest));
    dest = dest | src;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));

    CLEAR_FLAG_CF;
    CLEAR_FLAG_OF;

    DI_UPDATE_FLAGS_PSZ(DI_MEM_SET_DEST(dest));

    // UNDOCUMENTED ***************
    CLEAR_FLAG_AF;
    // ****************************

    DI_PROC_END
}

int Diana_Call_enter(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    // DEST 
    OPERAND_SIZE rbp = 0, rsp = 0, tempRBP = 0;

    DI_DEF_LOCALS(op1, op2);
    oldDestValue;
    DI_MEM_GET_DEST(op1);
    DI_MEM_GET_SRC(op2);

    if (pCallContext->m_context.iAMD64Mode)
    {
        // In 64-bit mode, the operand size of ENTER defaults to 64 bits, 
        // and there is no prefix available for
        // encoding a 32-bit operand size. (c)
        pCallContext->m_context.iCurrentCmd_opsize = 8;
    }
    // push rbp
    rbp = GET_REG_RBP2(pCallContext->m_context.iCurrentCmd_opsize);
    DI_CHECK(diana_internal_push(pCallContext, &rbp));

    // tempRBP <- esp
    tempRBP = GET_REG_RSP2(pCallContext->m_context.iCurrentCmd_opsize);

    // push frames
    op2 &= 0x1FULL;
    if (op2 > 0)
    {
        int i = 0;
        OPERAND_SIZE selector = GET_REG_SS;
        OPERAND_SIZE tmp = 0;
        tmp = rbp - pCallContext->m_context.iCurrentCmd_opsize;
        for(i =1; i < op2; ++i)
        {
            OPERAND_SIZE value = 0;
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                                selector,
                                                tmp,
                                                pCallContext->m_context.iCurrentCmd_opsize,
                                                &value,
                                                0,
                                                reg_SS));
            DI_CHECK(diana_internal_push(pCallContext, &value));
            tmp -= pCallContext->m_context.iCurrentCmd_opsize;
        }

        // push tempRBP
        DI_CHECK(diana_internal_push(pCallContext, &tempRBP));
    }
    // mov ebp, esp        
    SET_REG_RBP2(tempRBP, pCallContext->m_context.iCurrentCmd_opsize);

    // sub esp, N
    rsp = GET_REG_RSP2(pCallContext->m_context.iCurrentCmd_opsize);
    SET_REG_RSP2(rsp - op1,
                 pCallContext->m_context.iCurrentCmd_opsize);
    
    DI_PROC_END
}

static
int Diana_Call_xlat(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    //IF AddressSize = 16
    //THEN
    //   AL := (BX + ZeroExtend(AL))
    //ELSE (* AddressSize = 32 *)
    //   AL := (EBX + ZeroExtend(AL));
    //FI;     
    OPERAND_SIZE al = 0, dest = 0, result = 0;

    dest = GET_REG_RBX2(pDianaContext->iCurrentCmd_addressSize);
    al = GET_REG_AL;

    dest = dest + al;
    
    DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                        GET_REG_DS,
                                        dest,
                                        1,
                                        &result,
                                        0,
                                        reg_DS));
    SET_REG_AL(result);

    DI_PROC_END
}

static
int Diana_Call_smsw(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    //DEST := CR0
    switch (pCallContext->m_result.linkedOperands[0].type)
    {
    case diana_index:
        {
            DI_DEF_LOCAL_1(src_dest);
            DI_MEM_GET_DEST(src_dest);

            src_dest = GET_REG_CR0 & 0xFFFF;
            src_dest_size = 2;

            DI_MEM_SET_DEST(src_dest);
        }
        break;
    case diana_register:
        {
            DI_DEF_LOCAL_1(src_dest);
            DI_MEM_GET_DEST(src_dest);

            src_dest = (src_dest & ~0xFFFFULL) | (GET_REG_CR0 & 0xFFFFULL);

            DI_MEM_SET_DEST(src_dest);
        }
        break;
    default:
        return DI_ERROR;
    }

    DI_PROC_END
}

int Diana_Call_do_nothing(struct _dianaContext * pDianaContext,
                          DianaProcessor * pCallContext)
{
    &pDianaContext;
    &pCallContext;
    DI_PROC_END
}
// ---------------  DONE ------------------------------
static
void DianaProcessor_OnGroup(DianaGroupInfo * p)
{
    switch (p->m_commandId)
    {
    DI_PROC_REGISTER_COMMAND(aaa)
    DI_PROC_REGISTER_COMMAND(aad)
    DI_PROC_REGISTER_COMMAND(aam)
    DI_PROC_REGISTER_COMMAND(aas)
    DI_PROC_REGISTER_COMMAND(adc)
    DI_PROC_REGISTER_COMMAND(add)
    DI_PROC_REGISTER_COMMAND(and)

    DI_PROC_REGISTER_COMMAND(bsf)
    DI_PROC_REGISTER_COMMAND(bsr)
    DI_PROC_REGISTER_COMMAND(bt)
    DI_PROC_REGISTER_COMMAND(btc)
    DI_PROC_REGISTER_COMMAND(btr)
    DI_PROC_REGISTER_COMMAND(bts)
    DI_PROC_REGISTER_COMMAND(call)
    DI_PROC_REGISTER_COMMAND(cbw)
    DI_PROC_REGISTER_COMMAND(clc)
    DI_PROC_REGISTER_COMMAND(cld)
    DI_PROC_REGISTER_COMMAND(cli)
    DI_PROC_REGISTER_COMMAND(cmc)
    DI_PROC_REGISTER_COMMAND(cmova)
    DI_PROC_REGISTER_COMMAND(cmovae)
    DI_PROC_REGISTER_COMMAND(cmovb)
    DI_PROC_REGISTER_COMMAND(cmovbe)
    DI_PROC_REGISTER_COMMAND(cmove)
    DI_PROC_REGISTER_COMMAND(cmovg)
    DI_PROC_REGISTER_COMMAND(cmovge)
    DI_PROC_REGISTER_COMMAND(cmovl)
    DI_PROC_REGISTER_COMMAND(cmovle)
    DI_PROC_REGISTER_COMMAND(cmovne)
    DI_PROC_REGISTER_COMMAND(cmovno)
    DI_PROC_REGISTER_COMMAND(cmovnp)
    DI_PROC_REGISTER_COMMAND(cmovns)
    DI_PROC_REGISTER_COMMAND(cmovo)
    DI_PROC_REGISTER_COMMAND(cmovp)
    DI_PROC_REGISTER_COMMAND(cmovs)
    DI_PROC_REGISTER_COMMAND(cmp)
    DI_PROC_REGISTER_COMMAND(cmps)
    DI_PROC_REGISTER_COMMAND(cmpxchg)
    DI_PROC_REGISTER_COMMAND(cmpxchg16b)

    DI_PROC_REGISTER_COMMAND(cpuid)

    DI_PROC_REGISTER_COMMAND(daa)
    DI_PROC_REGISTER_COMMAND(das)
    DI_PROC_REGISTER_COMMAND(dec)
    DI_PROC_REGISTER_COMMAND(div)

    DI_PROC_REGISTER_COMMAND(idiv)
    DI_PROC_REGISTER_COMMAND(imul)
    DI_PROC_REGISTER_COMMAND(inc)

    DI_PROC_REGISTER_COMMAND(ja)
    DI_PROC_REGISTER_COMMAND(jae)
    DI_PROC_REGISTER_COMMAND(jb)
    DI_PROC_REGISTER_COMMAND(jbe)
    DI_PROC_REGISTER_COMMAND(je)
    DI_PROC_REGISTER_COMMAND(jg)
    DI_PROC_REGISTER_COMMAND(jge)
    DI_PROC_REGISTER_COMMAND(jl)
    DI_PROC_REGISTER_COMMAND(jle)
    DI_PROC_REGISTER_COMMAND(jne)
    DI_PROC_REGISTER_COMMAND(jno)
    DI_PROC_REGISTER_COMMAND(jnp)
    DI_PROC_REGISTER_COMMAND(jns)
    DI_PROC_REGISTER_COMMAND(jo)
    DI_PROC_REGISTER_COMMAND(jp)
    DI_PROC_REGISTER_COMMAND(js)
    DI_PROC_REGISTER_COMMAND(jecxz)
    DI_PROC_REGISTER_COMMAND(jmp)

    DI_PROC_REGISTER_COMMAND(lahf)
    DI_PROC_REGISTER_COMMAND(lea)
    DI_PROC_REGISTER_COMMAND(leave)
    DI_PROC_REGISTER_COMMAND(lods)
    DI_PROC_REGISTER_COMMAND(lfs)
    DI_PROC_REGISTER_COMMAND(lgs)
    DI_PROC_REGISTER_COMMAND(lss)
    DI_PROC_REGISTER_COMMAND(lds)
    DI_PROC_REGISTER_COMMAND(les)

    DI_PROC_REGISTER_COMMAND(loop)
    DI_PROC_REGISTER_COMMAND(loope)
    DI_PROC_REGISTER_COMMAND(loopne)

    DI_PROC_REGISTER_COMMAND(mov)
    DI_PROC_REGISTER_COMMAND(movs)
    DI_PROC_REGISTER_COMMAND(movsx)
    DI_PROC_REGISTER_COMMAND(movsxd)
    DI_PROC_REGISTER_COMMAND(movzx)
    DI_PROC_REGISTER_COMMAND(mul)

    DI_PROC_REGISTER_COMMAND(neg)
    DI_PROC_REGISTER_COMMAND(not)
    DI_PROC_REGISTER_COMMAND(nop)
    DI_PROC_REGISTER_COMMAND(hint_nop)

    DI_PROC_REGISTER_COMMAND(or)

    DI_PROC_REGISTER_COMMAND(push)
    DI_PROC_REGISTER_COMMAND(pushf)
    DI_PROC_REGISTER_COMMAND(popf)
    DI_PROC_REGISTER_COMMAND(pop)
    DI_PROC_REGISTER_COMMAND(popa)
    DI_PROC_REGISTER_COMMAND(pusha)
    DI_PROC_REGISTER_COMMAND(pause)

    DI_PROC_REGISTER_COMMAND(rcl)
    DI_PROC_REGISTER_COMMAND(rcr)
    DI_PROC_REGISTER_COMMAND(rol)
    DI_PROC_REGISTER_COMMAND(ror)

    DI_PROC_REGISTER_COMMAND(rdrand)
    DI_PROC_REGISTER_COMMAND(rdseed)
    
    DI_PROC_REGISTER_COMMAND(sahf)
    DI_PROC_REGISTER_COMMAND(shl)
    DI_PROC_REGISTER_COMMAND(sar)
    DI_PROC_REGISTER_COMMAND(sbb)

    DI_PROC_REGISTER_COMMAND(stc)
    DI_PROC_REGISTER_COMMAND(sti)
    DI_PROC_REGISTER_COMMAND(std)
    DI_PROC_REGISTER_COMMAND(shld)
    DI_PROC_REGISTER_COMMAND(shr)
    DI_PROC_REGISTER_COMMAND(shrd)
    DI_PROC_REGISTER_COMMAND(scas)
    DI_PROC_REGISTER_COMMAND(stos)

    DI_PROC_REGISTER_COMMAND(seta)
    DI_PROC_REGISTER_COMMAND(setae)
    DI_PROC_REGISTER_COMMAND(setb)
    DI_PROC_REGISTER_COMMAND(setbe)
    DI_PROC_REGISTER_COMMAND(sete)
    DI_PROC_REGISTER_COMMAND(setg)
    DI_PROC_REGISTER_COMMAND(setge)
    DI_PROC_REGISTER_COMMAND(setl)
    DI_PROC_REGISTER_COMMAND(setle)
    DI_PROC_REGISTER_COMMAND(setne)
    DI_PROC_REGISTER_COMMAND(setno)
    DI_PROC_REGISTER_COMMAND(setnp)
    DI_PROC_REGISTER_COMMAND(setns)
    DI_PROC_REGISTER_COMMAND(seto)
    DI_PROC_REGISTER_COMMAND(setp)
    DI_PROC_REGISTER_COMMAND(sets)

    DI_PROC_REGISTER_COMMAND(smsw)

    DI_PROC_REGISTER_COMMAND(enter)
    DI_PROC_REGISTER_COMMAND(cwd)
    DI_PROC_REGISTER_COMMAND(xor)
    DI_PROC_REGISTER_COMMAND(xadd)
    DI_PROC_REGISTER_COMMAND(xchg)
    DI_PROC_REGISTER_COMMAND(xlat)
    DI_PROC_REGISTER_COMMAND(test)
    DI_PROC_REGISTER_COMMAND(sub)
    DI_PROC_REGISTER_COMMAND(ret)
    DI_PROC_REGISTER_COMMAND(retf)

    DI_PROC_REGISTER_COMMAND(bswap)

    DI_PROC_REGISTER_COMMAND(lddqu)
    DI_PROC_REGISTER_COMMAND(movups)
    DI_PROC_REGISTER_COMMAND(movupd)
    DI_PROC_REGISTER_COMMAND(movdqu)
    DI_PROC_REGISTER_COMMAND(movd)
    DI_PROC_REGISTER_COMMAND(movaps)
    DI_PROC_REGISTER_COMMAND(movapd)
    DI_PROC_REGISTER_COMMAND(movdqa)
    DI_PROC_REGISTER_COMMAND(movntps)
    DI_PROC_REGISTER_COMMAND(movntpd)
    DI_PROC_REGISTER_COMMAND(movntdq)
    DI_PROC_REGISTER_COMMAND(movss)
    DI_PROC_REGISTER_COMMAND(movlpd)
    DI_PROC_REGISTER_COMMAND(movlps)
    DI_PROC_REGISTER_COMMAND(movsd)
    DI_PROC_REGISTER_COMMAND(movddup)

    DI_PROC_REGISTER_COMMAND(pxor)
    DI_PROC_REGISTER_COMMAND(por)
    DI_PROC_REGISTER_COMMAND(pand)
    DI_PROC_REGISTER_COMMAND(pandn)

    DI_PROC_REGISTER_COMMAND(wait)
    DI_PROC_REGISTER_COMMAND(fnstcw)
    DI_PROC_REGISTER_COMMAND(fnclex)
    DI_PROC_REGISTER_COMMAND(fldcw)
    DI_PROC_REGISTER_COMMAND(fild)
    DI_PROC_REGISTER_COMMAND(fmul)
    DI_PROC_REGISTER_COMMAND(fstp)
    DI_PROC_REGISTER_COMMAND(fst)
    DI_PROC_REGISTER_COMMAND(fld)
    DI_PROC_REGISTER_COMMAND(fcomp)
    DI_PROC_REGISTER_COMMAND(fnstsw)
    DI_PROC_REGISTER_COMMAND(fsqrt)
    DI_PROC_REGISTER_COMMAND(fcom)
    
    DI_PROC_REGISTER_COMMAND(fsub)
    DI_PROC_REGISTER_COMMAND(fsubp)
    DI_PROC_REGISTER_COMMAND(fsubr)
    DI_PROC_REGISTER_COMMAND(fsubrp)
    DI_PROC_REGISTER_COMMAND(fisub)
    DI_PROC_REGISTER_COMMAND(fisubr)

    DI_PROC_REGISTER_COMMAND(fadd)
    DI_PROC_REGISTER_COMMAND(faddp)
    DI_PROC_REGISTER_COMMAND(fiadd)

    DI_PROC_REGISTER_COMMAND(fcompp)
    DI_PROC_REGISTER_COMMAND(fist)
    DI_PROC_REGISTER_COMMAND(fistp)

    DI_PROC_REGISTER_COMMAND(fmulp)

    DI_PROC_REGISTER_COMMAND(fdiv)
    DI_PROC_REGISTER_COMMAND(fdivp)
    DI_PROC_REGISTER_COMMAND(fidiv)
    DI_PROC_REGISTER_COMMAND(fdivr)
    DI_PROC_REGISTER_COMMAND(fdivrp)
    DI_PROC_REGISTER_COMMAND(fidivr)

    /* FPU2: exchange, sign/abs, test/examine */
    DI_PROC_REGISTER_COMMAND(fxch)
    DI_PROC_REGISTER_COMMAND(fabs)
    DI_PROC_REGISTER_COMMAND(fchs)
    DI_PROC_REGISTER_COMMAND(ftst)
    DI_PROC_REGISTER_COMMAND(fxam)

    /* FPU2: unordered compare */
    DI_PROC_REGISTER_COMMAND(fucom)
    DI_PROC_REGISTER_COMMAND(fucomp)
    DI_PROC_REGISTER_COMMAND(fucompp)

    /* FPU2: compare and set EFLAGS */
    DI_PROC_REGISTER_COMMAND(fcomi)
    DI_PROC_REGISTER_COMMAND(fcomip)
    DI_PROC_REGISTER_COMMAND(fucomi)
    DI_PROC_REGISTER_COMMAND(fucomip)

    /* FPU2: integer compare */
    DI_PROC_REGISTER_COMMAND(ficom)
    DI_PROC_REGISTER_COMMAND(ficomp)

    /* FPU2: load constants */
    DI_PROC_REGISTER_COMMAND(fld1)
    DI_PROC_REGISTER_COMMAND(fldz)
    DI_PROC_REGISTER_COMMAND(fldpi)
    DI_PROC_REGISTER_COMMAND(fldl2e)
    DI_PROC_REGISTER_COMMAND(fldl2t)
    DI_PROC_REGISTER_COMMAND(fldlg2)
    DI_PROC_REGISTER_COMMAND(fldln2)

    /* FPU2: stack management */
    DI_PROC_REGISTER_COMMAND(fnop)
    DI_PROC_REGISTER_COMMAND(fdecstp)
    DI_PROC_REGISTER_COMMAND(fincstp)
    DI_PROC_REGISTER_COMMAND(ffree)
    DI_PROC_REGISTER_COMMAND(ffreep)
    DI_PROC_REGISTER_COMMAND(fninit)

    /* FPU2: arithmetic */
    DI_PROC_REGISTER_COMMAND(fimul)
    DI_PROC_REGISTER_COMMAND(frndint)
    DI_PROC_REGISTER_COMMAND(fscale)
    DI_PROC_REGISTER_COMMAND(fxtract)
    DI_PROC_REGISTER_COMMAND(f2xm1)
    DI_PROC_REGISTER_COMMAND(fyl2x)
    DI_PROC_REGISTER_COMMAND(fyl2xp1)
    DI_PROC_REGISTER_COMMAND(fpatan)
    DI_PROC_REGISTER_COMMAND(fptan)
    DI_PROC_REGISTER_COMMAND(fprem)
    DI_PROC_REGISTER_COMMAND(fprem1)
    DI_PROC_REGISTER_COMMAND(fsin)
    DI_PROC_REGISTER_COMMAND(fcos)
    DI_PROC_REGISTER_COMMAND(fsincos)

    /* FPU2: store integer truncated */
    DI_PROC_REGISTER_COMMAND(fisttp)

    /* FPU2: conditional float move */
    DI_PROC_REGISTER_COMMAND(fcmovb)
    DI_PROC_REGISTER_COMMAND(fcmove)
    DI_PROC_REGISTER_COMMAND(fcmovbe)
    DI_PROC_REGISTER_COMMAND(fcmovu)
    DI_PROC_REGISTER_COMMAND(fcmovnb)
    DI_PROC_REGISTER_COMMAND(fcmovne)
    DI_PROC_REGISTER_COMMAND(fcmovnbe)
    DI_PROC_REGISTER_COMMAND(fcmovnu)

    /* FPU2: complex state */
    DI_PROC_REGISTER_COMMAND(fbld)
    DI_PROC_REGISTER_COMMAND(fbstp)
    DI_PROC_REGISTER_COMMAND(fldenv)
    DI_PROC_REGISTER_COMMAND(fnstenv)
    DI_PROC_REGISTER_COMMAND(frstor)
    DI_PROC_REGISTER_COMMAND(fnsave)
    DI_PROC_REGISTER_COMMAND(fxsave)
    DI_PROC_REGISTER_COMMAND(fxrstor)

    DI_PROC_REGISTER_COMMAND_EX(prefetchnta, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(prefetcht0, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(prefetcht1, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(prefetcht2, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(prefetch, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(prefetchw, Diana_Call_do_nothing)

    // sse
    DI_PROC_REGISTER_COMMAND(addsd)
    DI_PROC_REGISTER_COMMAND(comisd)
    DI_PROC_REGISTER_COMMAND(subsd)
    DI_PROC_REGISTER_COMMAND(divsd)
    DI_PROC_REGISTER_COMMAND(mulsd)
    DI_PROC_REGISTER_COMMAND(cvttsd2si)
    DI_PROC_REGISTER_COMMAND(xgetbv)
    DI_PROC_REGISTER_COMMAND(pshufd)

    DI_PROC_REGISTER_COMMAND(punpcklbw)
    DI_PROC_REGISTER_COMMAND(punpcklwd)
    DI_PROC_REGISTER_COMMAND(punpckldq)
    DI_PROC_REGISTER_COMMAND(punpcklqdq)

    DI_PROC_REGISTER_COMMAND(stmxcsr)

    // stream
    DI_PROC_REGISTER_COMMAND(andpd)
    DI_PROC_REGISTER_COMMAND(andps)
    DI_PROC_REGISTER_COMMAND(andnpd)
    DI_PROC_REGISTER_COMMAND(andnps)
    DI_PROC_REGISTER_COMMAND(orpd)
    DI_PROC_REGISTER_COMMAND(orps)
    DI_PROC_REGISTER_COMMAND(xorpd)
    DI_PROC_REGISTER_COMMAND(xorps)
        
    DI_PROC_REGISTER_COMMAND(movhps)

    /* SSE2: packed integer add */
    DI_PROC_REGISTER_COMMAND(paddb)
    DI_PROC_REGISTER_COMMAND(paddw)
    DI_PROC_REGISTER_COMMAND(paddd)
    DI_PROC_REGISTER_COMMAND(paddq)
    DI_PROC_REGISTER_COMMAND(paddsb)
    DI_PROC_REGISTER_COMMAND(paddsw)
    DI_PROC_REGISTER_COMMAND(paddusb)
    DI_PROC_REGISTER_COMMAND(paddusw)

    /* SSE2: packed integer subtract */
    DI_PROC_REGISTER_COMMAND(psubb)
    DI_PROC_REGISTER_COMMAND(psubw)
    DI_PROC_REGISTER_COMMAND(psubd)
    DI_PROC_REGISTER_COMMAND(psubq)
    DI_PROC_REGISTER_COMMAND(psubsb)
    DI_PROC_REGISTER_COMMAND(psubsw)
    DI_PROC_REGISTER_COMMAND(psubusb)
    DI_PROC_REGISTER_COMMAND(psubusw)

    /* SSE2: packed compare */
    DI_PROC_REGISTER_COMMAND(pcmpeqb)
    DI_PROC_REGISTER_COMMAND(pcmpeqd)
    DI_PROC_REGISTER_COMMAND(pcmpeqw)
    DI_PROC_REGISTER_COMMAND(pcmpgtb)
    DI_PROC_REGISTER_COMMAND(pcmpgtd)
    DI_PROC_REGISTER_COMMAND(pcmpgtw)

    /* SSE2: packed multiply */
    DI_PROC_REGISTER_COMMAND(pmullw)
    DI_PROC_REGISTER_COMMAND(pmulhw)
    DI_PROC_REGISTER_COMMAND(pmulhuw)
    DI_PROC_REGISTER_COMMAND(pmuludq)
    DI_PROC_REGISTER_COMMAND(pmaddwd)

    /* SSE2: packed average/min/max */
    DI_PROC_REGISTER_COMMAND(pavgb)
    DI_PROC_REGISTER_COMMAND(pavgw)
    DI_PROC_REGISTER_COMMAND(pmaxsw)
    DI_PROC_REGISTER_COMMAND(pmaxub)
    DI_PROC_REGISTER_COMMAND(pminsw)
    DI_PROC_REGISTER_COMMAND(pminub)
    DI_PROC_REGISTER_COMMAND(psadbw)

    /* SSE2: mask / insert / extract */
    DI_PROC_REGISTER_COMMAND(pmovmskb)
    DI_PROC_REGISTER_COMMAND(pinsrw)
    DI_PROC_REGISTER_COMMAND(pextrw)

    /* SSE2: pack */
    DI_PROC_REGISTER_COMMAND(packssdw)
    DI_PROC_REGISTER_COMMAND(packsswb)
    DI_PROC_REGISTER_COMMAND(packuswb)

    /* SSE2: unpack high */
    DI_PROC_REGISTER_COMMAND(punpckhbw)
    DI_PROC_REGISTER_COMMAND(punpckhdq)
    DI_PROC_REGISTER_COMMAND(punpckhqdq)
    DI_PROC_REGISTER_COMMAND(punpckhwd)

    /* SSE2: shifts */
    DI_PROC_REGISTER_COMMAND(psllw)
    DI_PROC_REGISTER_COMMAND(pslld)
    DI_PROC_REGISTER_COMMAND(psllq)
    DI_PROC_REGISTER_COMMAND(pslldq)
    DI_PROC_REGISTER_COMMAND(psrlw)
    DI_PROC_REGISTER_COMMAND(psrld)
    DI_PROC_REGISTER_COMMAND(psrlq)
    DI_PROC_REGISTER_COMMAND(psrldq)
    DI_PROC_REGISTER_COMMAND(psraw)
    DI_PROC_REGISTER_COMMAND(psrad)

    /* SSE2: shuffle */
    DI_PROC_REGISTER_COMMAND(pshufhw)
    DI_PROC_REGISTER_COMMAND(pshuflw)
    DI_PROC_REGISTER_COMMAND(pshufw)

    /* SSE2: move quadword */
    DI_PROC_REGISTER_COMMAND(movq)
    DI_PROC_REGISTER_COMMAND(movq2dq)
    DI_PROC_REGISTER_COMMAND(movdq2q)

    /* SSE2: MMX state */
    DI_PROC_REGISTER_COMMAND(emms)
    DI_PROC_REGISTER_COMMAND(femms)

    /* SSE: scalar float32 */
    DI_PROC_REGISTER_COMMAND(addss)
    DI_PROC_REGISTER_COMMAND(subss)
    DI_PROC_REGISTER_COMMAND(mulss)
    DI_PROC_REGISTER_COMMAND(divss)
    DI_PROC_REGISTER_COMMAND(sqrtss)
    DI_PROC_REGISTER_COMMAND(comiss)
    DI_PROC_REGISTER_COMMAND(ucomiss)
    DI_PROC_REGISTER_COMMAND(cvtsi2ss)
    DI_PROC_REGISTER_COMMAND(cvtss2sd)
    DI_PROC_REGISTER_COMMAND(cvtss2si)
    DI_PROC_REGISTER_COMMAND(cvttss2si)
    DI_PROC_REGISTER_COMMAND(maxss)
    DI_PROC_REGISTER_COMMAND(minss)

    /* SSE: packed float32 */
    DI_PROC_REGISTER_COMMAND(addps)
    DI_PROC_REGISTER_COMMAND(subps)
    DI_PROC_REGISTER_COMMAND(mulps)
    DI_PROC_REGISTER_COMMAND(divps)
    DI_PROC_REGISTER_COMMAND(sqrtps)
    DI_PROC_REGISTER_COMMAND(maxps)
    DI_PROC_REGISTER_COMMAND(minps)

    /* SSE2: packed float64 */
    DI_PROC_REGISTER_COMMAND(addpd)
    DI_PROC_REGISTER_COMMAND(subpd)
    DI_PROC_REGISTER_COMMAND(mulpd)
    DI_PROC_REGISTER_COMMAND(divpd)
    DI_PROC_REGISTER_COMMAND(sqrtpd)
    DI_PROC_REGISTER_COMMAND(maxpd)
    DI_PROC_REGISTER_COMMAND(minpd)

    /* SSE2: scalar float64 extras */
    DI_PROC_REGISTER_COMMAND(sqrtsd)
    DI_PROC_REGISTER_COMMAND(ucomisd)
    DI_PROC_REGISTER_COMMAND(maxsd)
    DI_PROC_REGISTER_COMMAND(minsd)
    DI_PROC_REGISTER_COMMAND(cvtsi2sd)
    DI_PROC_REGISTER_COMMAND(cvtsd2ss)
    DI_PROC_REGISTER_COMMAND(cvtsd2si)

    /* SSE3: move with duplication */
    DI_PROC_REGISTER_COMMAND(movshdup)
    DI_PROC_REGISTER_COMMAND(movsldup)

    /* SSE: unpack */
    DI_PROC_REGISTER_COMMAND(unpcklps)
    DI_PROC_REGISTER_COMMAND(unpckhps)
    DI_PROC_REGISTER_COMMAND(unpcklpd)
    DI_PROC_REGISTER_COMMAND(unpckhpd)

    /* SSE: shuffle */
    DI_PROC_REGISTER_COMMAND(shufps)
    DI_PROC_REGISTER_COMMAND(shufpd)

    /* SSE: MXCSR */
    DI_PROC_REGISTER_COMMAND(ldmxcsr)

    /* System */
    DI_PROC_REGISTER_COMMAND(rdtsc)
    DI_PROC_REGISTER_COMMAND(rdtscp)
    DI_PROC_REGISTER_COMMAND(mfence)
    DI_PROC_REGISTER_COMMAND(lfence)
    DI_PROC_REGISTER_COMMAND(sfence)
    DI_PROC_REGISTER_COMMAND(clflush)
    DI_PROC_REGISTER_COMMAND(ud2)

    /* the smaller gaps (plans/done/x86-small-items.md) */
    DI_PROC_REGISTER_COMMAND(movhlps)
    DI_PROC_REGISTER_COMMAND(movlhps)
    DI_PROC_REGISTER_COMMAND(xsave)
    DI_PROC_REGISTER_COMMAND(xsaveopt)
    DI_PROC_REGISTER_COMMAND(xsavec)
    DI_PROC_REGISTER_COMMAND(xrstor)
    DI_PROC_REGISTER_COMMAND(rdsspd)
    DI_PROC_REGISTER_COMMAND(xbegin)
    DI_PROC_REGISTER_COMMAND(xabort)
    DI_PROC_REGISTER_COMMAND(xend)
    DI_PROC_REGISTER_COMMAND(xtest)
    DI_PROC_REGISTER_COMMAND(rdpkru)
    DI_PROC_REGISTER_COMMAND(wrpkru)
    DI_PROC_REGISTER_COMMAND_EX(movnti, Diana_Call_mov)
    DI_PROC_REGISTER_COMMAND_EX(movdiri, Diana_Call_mov)
    DI_PROC_REGISTER_COMMAND_EX(prefetchwt1, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(cldemote, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(clwb, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(clflushopt, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(serialize, Diana_Call_do_nothing)
    DI_PROC_REGISTER_COMMAND_EX(ud0, Diana_Call_ud2)
    DI_PROC_REGISTER_COMMAND_EX(ud1, Diana_Call_ud2)
    DI_PROC_REGISTER_COMMAND_EX(incsspd, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(saveprevssp, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(rstorssp, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(setssbsy, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(clrssbsy, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(wrssd, Diana_Call_cet_ud)
    DI_PROC_REGISTER_COMMAND_EX(wrussd, Diana_Call_cet_ud)

    /* general-register extensions: POPCNT, LZCNT, TZCNT, BMI1, BMI2, CRC32, MOVBE, ADX, CET */
    DI_PROC_REGISTER_COMMAND(endbr32)
    DI_PROC_REGISTER_COMMAND(endbr64)
    DI_PROC_REGISTER_COMMAND(popcnt)
    DI_PROC_REGISTER_COMMAND(lzcnt)
    DI_PROC_REGISTER_COMMAND(tzcnt)
    DI_PROC_REGISTER_COMMAND(andn)
    DI_PROC_REGISTER_COMMAND(bextr)
    DI_PROC_REGISTER_COMMAND(blsi)
    DI_PROC_REGISTER_COMMAND(blsmsk)
    DI_PROC_REGISTER_COMMAND(blsr)
    DI_PROC_REGISTER_COMMAND(bzhi)
    DI_PROC_REGISTER_COMMAND(mulx)
    DI_PROC_REGISTER_COMMAND(pdep)
    DI_PROC_REGISTER_COMMAND(pext)
    DI_PROC_REGISTER_COMMAND(rorx)
    DI_PROC_REGISTER_COMMAND(sarx)
    DI_PROC_REGISTER_COMMAND(shlx)
    DI_PROC_REGISTER_COMMAND(shrx)
    DI_PROC_REGISTER_COMMAND(crc32)
    DI_PROC_REGISTER_COMMAND(movbe)
    DI_PROC_REGISTER_COMMAND(adcx)
    DI_PROC_REGISTER_COMMAND(adox)

    /* SSE, SSE2, SSE3, SSE4.1, SSE4.2 */
    DI_PROC_REGISTER_COMMAND(cmpps)
    DI_PROC_REGISTER_COMMAND(cmpss)
    DI_PROC_REGISTER_COMMAND(cmppd)
    DI_PROC_REGISTER_COMMAND(cmpsd)
    DI_PROC_REGISTER_COMMAND(addsubps)
    DI_PROC_REGISTER_COMMAND(addsubpd)
    DI_PROC_REGISTER_COMMAND(haddps)
    DI_PROC_REGISTER_COMMAND(haddpd)
    DI_PROC_REGISTER_COMMAND(hsubps)
    DI_PROC_REGISTER_COMMAND(hsubpd)
    DI_PROC_REGISTER_COMMAND(cvtdq2ps)
    DI_PROC_REGISTER_COMMAND(cvtdq2pd)
    DI_PROC_REGISTER_COMMAND(cvtps2dq)
    DI_PROC_REGISTER_COMMAND(cvttps2dq)
    DI_PROC_REGISTER_COMMAND(cvtpd2dq)
    DI_PROC_REGISTER_COMMAND(cvttpd2dq)
    DI_PROC_REGISTER_COMMAND(cvtps2pd)
    DI_PROC_REGISTER_COMMAND(cvtpd2ps)
    DI_PROC_REGISTER_COMMAND(cvtps2pi)
    DI_PROC_REGISTER_COMMAND(cvttps2pi)
    DI_PROC_REGISTER_COMMAND(cvtpd2pi)
    DI_PROC_REGISTER_COMMAND(cvttpd2pi)
    DI_PROC_REGISTER_COMMAND(cvtpi2ps)
    DI_PROC_REGISTER_COMMAND(cvtpi2pd)
    DI_PROC_REGISTER_COMMAND(roundps)
    DI_PROC_REGISTER_COMMAND(roundpd)
    DI_PROC_REGISTER_COMMAND(roundss)
    DI_PROC_REGISTER_COMMAND(roundsd)
    DI_PROC_REGISTER_COMMAND(dpps)
    DI_PROC_REGISTER_COMMAND(dppd)
    DI_PROC_REGISTER_COMMAND(movmskps)
    DI_PROC_REGISTER_COMMAND(movmskpd)
    DI_PROC_REGISTER_COMMAND(movhpd)
    DI_PROC_REGISTER_COMMAND(movntq)
    DI_PROC_REGISTER_COMMAND(movntdqa)
    DI_PROC_REGISTER_COMMAND(maskmovdqu)
    DI_PROC_REGISTER_COMMAND(maskmovq)
    DI_PROC_REGISTER_COMMAND(blendps)
    DI_PROC_REGISTER_COMMAND(blendpd)
    DI_PROC_REGISTER_COMMAND(pblendw)
    DI_PROC_REGISTER_COMMAND(extractps)
    DI_PROC_REGISTER_COMMAND(pextrb)
    DI_PROC_REGISTER_COMMAND(pextrd)
    DI_PROC_REGISTER_COMMAND(pinsrb)
    DI_PROC_REGISTER_COMMAND(pinsrd)
    DI_PROC_REGISTER_COMMAND(insertps)
    DI_PROC_REGISTER_COMMAND(pmovsxbw)
    DI_PROC_REGISTER_COMMAND(pmovsxbd)
    DI_PROC_REGISTER_COMMAND(pmovsxwd)
    DI_PROC_REGISTER_COMMAND(pmovsxwq)
    DI_PROC_REGISTER_COMMAND(pmovsxdq)
    DI_PROC_REGISTER_COMMAND(pmovzxbw)
    DI_PROC_REGISTER_COMMAND(pmovzxbd)
    DI_PROC_REGISTER_COMMAND(pmovzxwd)
    DI_PROC_REGISTER_COMMAND(pmovzxwq)
    DI_PROC_REGISTER_COMMAND(pmovzxdq)
    DI_PROC_REGISTER_COMMAND(palignr)
    DI_PROC_REGISTER_COMMAND(pblendvb)
    DI_PROC_REGISTER_COMMAND(blendvps)
    DI_PROC_REGISTER_COMMAND(blendvpd)
    DI_PROC_REGISTER_COMMAND(pshufb)
    DI_PROC_REGISTER_COMMAND(phaddw)
    DI_PROC_REGISTER_COMMAND(phaddd)
    DI_PROC_REGISTER_COMMAND(phaddsw)
    DI_PROC_REGISTER_COMMAND(pmaddubsw)
    DI_PROC_REGISTER_COMMAND(phsubw)
    DI_PROC_REGISTER_COMMAND(phsubd)
    DI_PROC_REGISTER_COMMAND(phsubsw)
    DI_PROC_REGISTER_COMMAND(psignb)
    DI_PROC_REGISTER_COMMAND(psignw)
    DI_PROC_REGISTER_COMMAND(psignd)
    DI_PROC_REGISTER_COMMAND(pmulhrsw)
    DI_PROC_REGISTER_COMMAND(pabsb)
    DI_PROC_REGISTER_COMMAND(pabsw)
    DI_PROC_REGISTER_COMMAND(pabsd)
    DI_PROC_REGISTER_COMMAND(pmovsxbq)
    DI_PROC_REGISTER_COMMAND(pmovzxbq)
    DI_PROC_REGISTER_COMMAND(pminsb)
    DI_PROC_REGISTER_COMMAND(pminsd)
    DI_PROC_REGISTER_COMMAND(pminuw)
    DI_PROC_REGISTER_COMMAND(pminud)
    DI_PROC_REGISTER_COMMAND(pmaxsb)
    DI_PROC_REGISTER_COMMAND(pmaxsd)
    DI_PROC_REGISTER_COMMAND(pmaxuw)
    DI_PROC_REGISTER_COMMAND(pmaxud)
    DI_PROC_REGISTER_COMMAND(pcmpeqq)
    DI_PROC_REGISTER_COMMAND(pcmpgtq)
    DI_PROC_REGISTER_COMMAND(pmuldq)
    DI_PROC_REGISTER_COMMAND(pmulld)
    DI_PROC_REGISTER_COMMAND(packusdw)
    DI_PROC_REGISTER_COMMAND(phminposuw)
    DI_PROC_REGISTER_COMMAND(mpsadbw)
    DI_PROC_REGISTER_COMMAND(ptest)
    DI_PROC_REGISTER_COMMAND(pcmpestri)
    DI_PROC_REGISTER_COMMAND(pcmpestrm)
    DI_PROC_REGISTER_COMMAND(pcmpistri)
    DI_PROC_REGISTER_COMMAND(pcmpistrm)

    /* AES-NI, PCLMULQDQ, SHA */
    DI_PROC_REGISTER_COMMAND(aesenc)
    DI_PROC_REGISTER_COMMAND(aesenclast)
    DI_PROC_REGISTER_COMMAND(aesdec)
    DI_PROC_REGISTER_COMMAND(aesdeclast)
    DI_PROC_REGISTER_COMMAND(aesimc)
    DI_PROC_REGISTER_COMMAND(aeskeygenassist)
    DI_PROC_REGISTER_COMMAND(pclmulqdq)
    DI_PROC_REGISTER_COMMAND(sha1rnds4)
    DI_PROC_REGISTER_COMMAND(sha1nexte)
    DI_PROC_REGISTER_COMMAND(sha1msg1)
    DI_PROC_REGISTER_COMMAND(sha1msg2)
    DI_PROC_REGISTER_COMMAND(sha256msg1)
    DI_PROC_REGISTER_COMMAND(sha256msg2)
    DI_PROC_REGISTER_COMMAND(sha256rnds2)

    /* AVX: the VEX.128 forms */
    DI_PROC_REGISTER_COMMAND(vaddps)
    DI_PROC_REGISTER_COMMAND(vaddpd)
    DI_PROC_REGISTER_COMMAND(vaddss)
    DI_PROC_REGISTER_COMMAND(vaddsd)
    DI_PROC_REGISTER_COMMAND(vsubps)
    DI_PROC_REGISTER_COMMAND(vsubpd)
    DI_PROC_REGISTER_COMMAND(vsubss)
    DI_PROC_REGISTER_COMMAND(vsubsd)
    DI_PROC_REGISTER_COMMAND(vmulps)
    DI_PROC_REGISTER_COMMAND(vmulpd)
    DI_PROC_REGISTER_COMMAND(vmulss)
    DI_PROC_REGISTER_COMMAND(vmulsd)
    DI_PROC_REGISTER_COMMAND(vdivps)
    DI_PROC_REGISTER_COMMAND(vdivpd)
    DI_PROC_REGISTER_COMMAND(vdivss)
    DI_PROC_REGISTER_COMMAND(vdivsd)
    DI_PROC_REGISTER_COMMAND(vminps)
    DI_PROC_REGISTER_COMMAND(vminpd)
    DI_PROC_REGISTER_COMMAND(vminss)
    DI_PROC_REGISTER_COMMAND(vminsd)
    DI_PROC_REGISTER_COMMAND(vmaxps)
    DI_PROC_REGISTER_COMMAND(vmaxpd)
    DI_PROC_REGISTER_COMMAND(vmaxss)
    DI_PROC_REGISTER_COMMAND(vmaxsd)
    DI_PROC_REGISTER_COMMAND(vandps)
    DI_PROC_REGISTER_COMMAND(vandpd)
    DI_PROC_REGISTER_COMMAND(vandnps)
    DI_PROC_REGISTER_COMMAND(vandnpd)
    DI_PROC_REGISTER_COMMAND(vorps)
    DI_PROC_REGISTER_COMMAND(vorpd)
    DI_PROC_REGISTER_COMMAND(vxorps)
    DI_PROC_REGISTER_COMMAND(vxorpd)
    DI_PROC_REGISTER_COMMAND(vpand)
    DI_PROC_REGISTER_COMMAND(vpandn)
    DI_PROC_REGISTER_COMMAND(vpor)
    DI_PROC_REGISTER_COMMAND(vpxor)
    DI_PROC_REGISTER_COMMAND(vpaddb)
    DI_PROC_REGISTER_COMMAND(vpaddw)
    DI_PROC_REGISTER_COMMAND(vpaddd)
    DI_PROC_REGISTER_COMMAND(vpaddq)
    DI_PROC_REGISTER_COMMAND(vpsubb)
    DI_PROC_REGISTER_COMMAND(vpsubw)
    DI_PROC_REGISTER_COMMAND(vpsubd)
    DI_PROC_REGISTER_COMMAND(vpsubq)
    DI_PROC_REGISTER_COMMAND(vmovaps)
    DI_PROC_REGISTER_COMMAND(vmovapd)
    DI_PROC_REGISTER_COMMAND(vmovups)
    DI_PROC_REGISTER_COMMAND(vmovupd)
    DI_PROC_REGISTER_COMMAND(vmovddup)
    DI_PROC_REGISTER_COMMAND(vaesenc)
    DI_PROC_REGISTER_COMMAND(vaesenclast)
    DI_PROC_REGISTER_COMMAND(vaesdec)
    DI_PROC_REGISTER_COMMAND(vaesdeclast)
    DI_PROC_REGISTER_COMMAND(vaesimc)
    DI_PROC_REGISTER_COMMAND(vaeskeygenassist)
    DI_PROC_REGISTER_COMMAND(vpclmulqdq)
    /* AVX: added */
    DI_PROC_REGISTER_COMMAND(vpblendd)
    DI_PROC_REGISTER_COMMAND(vpbroadcastb)
    DI_PROC_REGISTER_COMMAND(vpbroadcastw)
    DI_PROC_REGISTER_COMMAND(vpbroadcastd)
    DI_PROC_REGISTER_COMMAND(vpbroadcastq)
    DI_PROC_REGISTER_COMMAND(vpermd)
    DI_PROC_REGISTER_COMMAND(vpermps)
    DI_PROC_REGISTER_COMMAND(vpermq)
    DI_PROC_REGISTER_COMMAND(vpermpd)
    DI_PROC_REGISTER_COMMAND(vpsllvd)
    DI_PROC_REGISTER_COMMAND(vpsllvq)
    DI_PROC_REGISTER_COMMAND(vpsrlvd)
    DI_PROC_REGISTER_COMMAND(vpsrlvq)
    DI_PROC_REGISTER_COMMAND(vpsravd)
    DI_PROC_REGISTER_COMMAND(vzeroupper)
    DI_PROC_REGISTER_COMMAND(vzeroall)
    DI_PROC_REGISTER_COMMAND(vmovd)
    DI_PROC_REGISTER_COMMAND(vmovq)
    DI_PROC_REGISTER_COMMAND(vmovss)
    DI_PROC_REGISTER_COMMAND(vmovsd)
    DI_PROC_REGISTER_COMMAND(vmovhps)
    DI_PROC_REGISTER_COMMAND(vmovhpd)
    DI_PROC_REGISTER_COMMAND(vmovlps)
    DI_PROC_REGISTER_COMMAND(vmovlpd)
    DI_PROC_REGISTER_COMMAND(vmovlhps)
    DI_PROC_REGISTER_COMMAND(vmovhlps)
    DI_PROC_REGISTER_COMMAND(vmovshdup)
    DI_PROC_REGISTER_COMMAND(vmovsldup)
    DI_PROC_REGISTER_COMMAND(vmovmskps)
    DI_PROC_REGISTER_COMMAND(vmovmskpd)
    DI_PROC_REGISTER_COMMAND(vldmxcsr)
    DI_PROC_REGISTER_COMMAND(vstmxcsr)
    DI_PROC_REGISTER_COMMAND(vmovdqa)
    DI_PROC_REGISTER_COMMAND(vmovdqu)
    DI_PROC_REGISTER_COMMAND(vmovntdq)
    DI_PROC_REGISTER_COMMAND(vmovntps)
    DI_PROC_REGISTER_COMMAND(vmovntpd)
    DI_PROC_REGISTER_COMMAND(vmovntdqa)
    DI_PROC_REGISTER_COMMAND(vlddqu)
    DI_PROC_REGISTER_COMMAND(vbroadcastss)
    DI_PROC_REGISTER_COMMAND(vbroadcastsd)
    DI_PROC_REGISTER_COMMAND(vbroadcastf128)
    DI_PROC_REGISTER_COMMAND(vinsertf128)
    DI_PROC_REGISTER_COMMAND(vextractf128)
    DI_PROC_REGISTER_COMMAND(vperm2f128)
    DI_PROC_REGISTER_COMMAND(vpermilps)
    DI_PROC_REGISTER_COMMAND(vpermilpd)
    DI_PROC_REGISTER_COMMAND(vshufps)
    DI_PROC_REGISTER_COMMAND(vshufpd)
    DI_PROC_REGISTER_COMMAND(vunpcklps)
    DI_PROC_REGISTER_COMMAND(vunpckhps)
    DI_PROC_REGISTER_COMMAND(vunpcklpd)
    DI_PROC_REGISTER_COMMAND(vunpckhpd)
    DI_PROC_REGISTER_COMMAND(vblendps)
    DI_PROC_REGISTER_COMMAND(vblendpd)
    DI_PROC_REGISTER_COMMAND(vtestps)
    DI_PROC_REGISTER_COMMAND(vtestpd)
    DI_PROC_REGISTER_COMMAND(vptest)
    DI_PROC_REGISTER_COMMAND(vmaskmovps)
    DI_PROC_REGISTER_COMMAND(vmaskmovpd)
    DI_PROC_REGISTER_COMMAND(vsqrtps)
    DI_PROC_REGISTER_COMMAND(vsqrtpd)
    DI_PROC_REGISTER_COMMAND(vsqrtss)
    DI_PROC_REGISTER_COMMAND(vsqrtsd)
    DI_PROC_REGISTER_COMMAND(vroundps)
    DI_PROC_REGISTER_COMMAND(vroundpd)
    DI_PROC_REGISTER_COMMAND(vroundss)
    DI_PROC_REGISTER_COMMAND(vroundsd)
    DI_PROC_REGISTER_COMMAND(vdpps)
    DI_PROC_REGISTER_COMMAND(vdppd)
    DI_PROC_REGISTER_COMMAND(vhaddps)
    DI_PROC_REGISTER_COMMAND(vhaddpd)
    DI_PROC_REGISTER_COMMAND(vhsubps)
    DI_PROC_REGISTER_COMMAND(vhsubpd)
    DI_PROC_REGISTER_COMMAND(vaddsubps)
    DI_PROC_REGISTER_COMMAND(vaddsubpd)
    DI_PROC_REGISTER_COMMAND(vcmpps)
    DI_PROC_REGISTER_COMMAND(vcmppd)
    DI_PROC_REGISTER_COMMAND(vcmpss)
    DI_PROC_REGISTER_COMMAND(vcmpsd)
    DI_PROC_REGISTER_COMMAND(vcomiss)
    DI_PROC_REGISTER_COMMAND(vcomisd)
    DI_PROC_REGISTER_COMMAND(vucomiss)
    DI_PROC_REGISTER_COMMAND(vucomisd)
    DI_PROC_REGISTER_COMMAND(vcvtdq2pd)
    DI_PROC_REGISTER_COMMAND(vcvtdq2ps)
    DI_PROC_REGISTER_COMMAND(vcvtpd2dq)
    DI_PROC_REGISTER_COMMAND(vcvttpd2dq)
    DI_PROC_REGISTER_COMMAND(vcvtpd2ps)
    DI_PROC_REGISTER_COMMAND(vcvtps2dq)
    DI_PROC_REGISTER_COMMAND(vcvttps2dq)
    DI_PROC_REGISTER_COMMAND(vcvtps2pd)
    DI_PROC_REGISTER_COMMAND(vcvtss2si)
    DI_PROC_REGISTER_COMMAND(vcvttss2si)
    DI_PROC_REGISTER_COMMAND(vcvtsd2si)
    DI_PROC_REGISTER_COMMAND(vcvttsd2si)
    DI_PROC_REGISTER_COMMAND(vcvtsi2ss)
    DI_PROC_REGISTER_COMMAND(vcvtsi2sd)
    DI_PROC_REGISTER_COMMAND(vcvtss2sd)
    DI_PROC_REGISTER_COMMAND(vcvtsd2ss)
    DI_PROC_REGISTER_COMMAND(vpackssdw)
    DI_PROC_REGISTER_COMMAND(vpacksswb)
    DI_PROC_REGISTER_COMMAND(vpackuswb)
    DI_PROC_REGISTER_COMMAND(vpackusdw)
    DI_PROC_REGISTER_COMMAND(vpaddsb)
    DI_PROC_REGISTER_COMMAND(vpaddsw)
    DI_PROC_REGISTER_COMMAND(vpaddusb)
    DI_PROC_REGISTER_COMMAND(vpaddusw)
    DI_PROC_REGISTER_COMMAND(vpsubsb)
    DI_PROC_REGISTER_COMMAND(vpsubsw)
    DI_PROC_REGISTER_COMMAND(vpsubusb)
    DI_PROC_REGISTER_COMMAND(vpsubusw)
    DI_PROC_REGISTER_COMMAND(vpavgb)
    DI_PROC_REGISTER_COMMAND(vpavgw)
    DI_PROC_REGISTER_COMMAND(vpcmpeqb)
    DI_PROC_REGISTER_COMMAND(vpcmpeqw)
    DI_PROC_REGISTER_COMMAND(vpcmpeqd)
    DI_PROC_REGISTER_COMMAND(vpcmpeqq)
    DI_PROC_REGISTER_COMMAND(vpcmpgtb)
    DI_PROC_REGISTER_COMMAND(vpcmpgtw)
    DI_PROC_REGISTER_COMMAND(vpcmpgtd)
    DI_PROC_REGISTER_COMMAND(vpcmpgtq)
    DI_PROC_REGISTER_COMMAND(vpmaddwd)
    DI_PROC_REGISTER_COMMAND(vpmaxsw)
    DI_PROC_REGISTER_COMMAND(vpmaxub)
    DI_PROC_REGISTER_COMMAND(vpminsw)
    DI_PROC_REGISTER_COMMAND(vpminub)
    DI_PROC_REGISTER_COMMAND(vpmaxsb)
    DI_PROC_REGISTER_COMMAND(vpmaxsd)
    DI_PROC_REGISTER_COMMAND(vpmaxuw)
    DI_PROC_REGISTER_COMMAND(vpmaxud)
    DI_PROC_REGISTER_COMMAND(vpminsb)
    DI_PROC_REGISTER_COMMAND(vpminsd)
    DI_PROC_REGISTER_COMMAND(vpminuw)
    DI_PROC_REGISTER_COMMAND(vpminud)
    DI_PROC_REGISTER_COMMAND(vpmulhuw)
    DI_PROC_REGISTER_COMMAND(vpmulhw)
    DI_PROC_REGISTER_COMMAND(vpmullw)
    DI_PROC_REGISTER_COMMAND(vpmuludq)
    DI_PROC_REGISTER_COMMAND(vpmulld)
    DI_PROC_REGISTER_COMMAND(vpmuldq)
    DI_PROC_REGISTER_COMMAND(vpsadbw)
    DI_PROC_REGISTER_COMMAND(vpunpckhbw)
    DI_PROC_REGISTER_COMMAND(vpunpckhwd)
    DI_PROC_REGISTER_COMMAND(vpunpckhdq)
    DI_PROC_REGISTER_COMMAND(vpunpckhqdq)
    DI_PROC_REGISTER_COMMAND(vpunpcklbw)
    DI_PROC_REGISTER_COMMAND(vpunpcklwd)
    DI_PROC_REGISTER_COMMAND(vpunpckldq)
    DI_PROC_REGISTER_COMMAND(vpunpcklqdq)
    DI_PROC_REGISTER_COMMAND(vpblendw)
    DI_PROC_REGISTER_COMMAND(vmpsadbw)
    DI_PROC_REGISTER_COMMAND(vinsertps)
    DI_PROC_REGISTER_COMMAND(vpinsrw)
    DI_PROC_REGISTER_COMMAND(vpinsrb)
    DI_PROC_REGISTER_COMMAND(vpinsrd)
    DI_PROC_REGISTER_COMMAND(vpshufd)
    DI_PROC_REGISTER_COMMAND(vpshufhw)
    DI_PROC_REGISTER_COMMAND(vpshuflw)
    DI_PROC_REGISTER_COMMAND(vphminposuw)
    DI_PROC_REGISTER_COMMAND(vpmovsxbw)
    DI_PROC_REGISTER_COMMAND(vpmovsxbd)
    DI_PROC_REGISTER_COMMAND(vpmovsxwd)
    DI_PROC_REGISTER_COMMAND(vpmovsxwq)
    DI_PROC_REGISTER_COMMAND(vpmovsxdq)
    DI_PROC_REGISTER_COMMAND(vpmovzxbw)
    DI_PROC_REGISTER_COMMAND(vpmovzxbd)
    DI_PROC_REGISTER_COMMAND(vpmovzxwd)
    DI_PROC_REGISTER_COMMAND(vpmovzxwq)
    DI_PROC_REGISTER_COMMAND(vpmovzxdq)
    DI_PROC_REGISTER_COMMAND(vpextrw)
    DI_PROC_REGISTER_COMMAND(vpextrb)
    DI_PROC_REGISTER_COMMAND(vpextrd)
    DI_PROC_REGISTER_COMMAND(vextractps)
    DI_PROC_REGISTER_COMMAND(vpmovmskb)
    DI_PROC_REGISTER_COMMAND(vmaskmovdqu)
    DI_PROC_REGISTER_COMMAND(vpcmpestri)
    DI_PROC_REGISTER_COMMAND(vpcmpistri)
    DI_PROC_REGISTER_COMMAND(vpcmpestrm)
    DI_PROC_REGISTER_COMMAND(vpcmpistrm)
    DI_PROC_REGISTER_COMMAND(vpsllw)
    DI_PROC_REGISTER_COMMAND(vpslld)
    DI_PROC_REGISTER_COMMAND(vpsllq)
    DI_PROC_REGISTER_COMMAND(vpsraw)
    DI_PROC_REGISTER_COMMAND(vpsrad)
    DI_PROC_REGISTER_COMMAND(vpsrlw)
    DI_PROC_REGISTER_COMMAND(vpsrld)
    DI_PROC_REGISTER_COMMAND(vpsrlq)
    DI_PROC_REGISTER_COMMAND(vpslldq)
    DI_PROC_REGISTER_COMMAND(vpsrldq)
    DI_PROC_REGISTER_COMMAND(vblendvps)
    DI_PROC_REGISTER_COMMAND(vblendvpd)
    DI_PROC_REGISTER_COMMAND(vpblendvb)
    DI_PROC_REGISTER_COMMAND(vpshufb)
    DI_PROC_REGISTER_COMMAND(vphaddw)
    DI_PROC_REGISTER_COMMAND(vphaddd)
    DI_PROC_REGISTER_COMMAND(vphaddsw)
    DI_PROC_REGISTER_COMMAND(vpmaddubsw)
    DI_PROC_REGISTER_COMMAND(vphsubw)
    DI_PROC_REGISTER_COMMAND(vphsubd)
    DI_PROC_REGISTER_COMMAND(vphsubsw)
    DI_PROC_REGISTER_COMMAND(vpsignb)
    DI_PROC_REGISTER_COMMAND(vpsignw)
    DI_PROC_REGISTER_COMMAND(vpsignd)
    DI_PROC_REGISTER_COMMAND(vpmulhrsw)
    DI_PROC_REGISTER_COMMAND(vpalignr)
    DI_PROC_REGISTER_COMMAND(vpabsb)
    DI_PROC_REGISTER_COMMAND(vpabsw)
    DI_PROC_REGISTER_COMMAND(vpabsd)
    DI_PROC_REGISTER_COMMAND(vpmovsxbq)
    DI_PROC_REGISTER_COMMAND(vpmovzxbq)
    DI_PROC_REGISTER_COMMAND(vcvtph2ps)
    DI_PROC_REGISTER_COMMAND(vcvtps2ph)
    DI_PROC_REGISTER_COMMAND(vbroadcasti128)
    DI_PROC_REGISTER_COMMAND(vinserti128)
    DI_PROC_REGISTER_COMMAND(vextracti128)
    DI_PROC_REGISTER_COMMAND(vperm2i128)
    DI_PROC_REGISTER_COMMAND(vpmaskmovd)
    DI_PROC_REGISTER_COMMAND(vpmaskmovq)
    DI_PROC_REGISTER_COMMAND(vgatherdps)
    DI_PROC_REGISTER_COMMAND(vgatherdpd)
    DI_PROC_REGISTER_COMMAND(vgatherqps)
    DI_PROC_REGISTER_COMMAND(vgatherqpd)
    DI_PROC_REGISTER_COMMAND(vpgatherdd)
    DI_PROC_REGISTER_COMMAND(vpgatherdq)
    DI_PROC_REGISTER_COMMAND(vpgatherqd)
    DI_PROC_REGISTER_COMMAND(vpgatherqq)
    DI_PROC_REGISTER_COMMAND(vfmaddsub132ps)
    DI_PROC_REGISTER_COMMAND(vfmaddsub132pd)
    DI_PROC_REGISTER_COMMAND(vfmaddsub213ps)
    DI_PROC_REGISTER_COMMAND(vfmaddsub213pd)
    DI_PROC_REGISTER_COMMAND(vfmaddsub231ps)
    DI_PROC_REGISTER_COMMAND(vfmaddsub231pd)
    DI_PROC_REGISTER_COMMAND(vfmsubadd132ps)
    DI_PROC_REGISTER_COMMAND(vfmsubadd132pd)
    DI_PROC_REGISTER_COMMAND(vfmsubadd213ps)
    DI_PROC_REGISTER_COMMAND(vfmsubadd213pd)
    DI_PROC_REGISTER_COMMAND(vfmsubadd231ps)
    DI_PROC_REGISTER_COMMAND(vfmsubadd231pd)
    DI_PROC_REGISTER_COMMAND(vfmadd132ps)
    DI_PROC_REGISTER_COMMAND(vfmadd132pd)
    DI_PROC_REGISTER_COMMAND(vfmadd132ss)
    DI_PROC_REGISTER_COMMAND(vfmadd132sd)
    DI_PROC_REGISTER_COMMAND(vfmadd213ps)
    DI_PROC_REGISTER_COMMAND(vfmadd213pd)
    DI_PROC_REGISTER_COMMAND(vfmadd213ss)
    DI_PROC_REGISTER_COMMAND(vfmadd213sd)
    DI_PROC_REGISTER_COMMAND(vfmadd231ps)
    DI_PROC_REGISTER_COMMAND(vfmadd231pd)
    DI_PROC_REGISTER_COMMAND(vfmadd231ss)
    DI_PROC_REGISTER_COMMAND(vfmadd231sd)
    DI_PROC_REGISTER_COMMAND(vfmsub132ps)
    DI_PROC_REGISTER_COMMAND(vfmsub132pd)
    DI_PROC_REGISTER_COMMAND(vfmsub132ss)
    DI_PROC_REGISTER_COMMAND(vfmsub132sd)
    DI_PROC_REGISTER_COMMAND(vfmsub213ps)
    DI_PROC_REGISTER_COMMAND(vfmsub213pd)
    DI_PROC_REGISTER_COMMAND(vfmsub213ss)
    DI_PROC_REGISTER_COMMAND(vfmsub213sd)
    DI_PROC_REGISTER_COMMAND(vfmsub231ps)
    DI_PROC_REGISTER_COMMAND(vfmsub231pd)
    DI_PROC_REGISTER_COMMAND(vfmsub231ss)
    DI_PROC_REGISTER_COMMAND(vfmsub231sd)
    DI_PROC_REGISTER_COMMAND(vfnmadd132ps)
    DI_PROC_REGISTER_COMMAND(vfnmadd132pd)
    DI_PROC_REGISTER_COMMAND(vfnmadd132ss)
    DI_PROC_REGISTER_COMMAND(vfnmadd132sd)
    DI_PROC_REGISTER_COMMAND(vfnmadd213ps)
    DI_PROC_REGISTER_COMMAND(vfnmadd213pd)
    DI_PROC_REGISTER_COMMAND(vfnmadd213ss)
    DI_PROC_REGISTER_COMMAND(vfnmadd213sd)
    DI_PROC_REGISTER_COMMAND(vfnmadd231ps)
    DI_PROC_REGISTER_COMMAND(vfnmadd231pd)
    DI_PROC_REGISTER_COMMAND(vfnmadd231ss)
    DI_PROC_REGISTER_COMMAND(vfnmadd231sd)
    DI_PROC_REGISTER_COMMAND(vfnmsub132ps)
    DI_PROC_REGISTER_COMMAND(vfnmsub132pd)
    DI_PROC_REGISTER_COMMAND(vfnmsub132ss)
    DI_PROC_REGISTER_COMMAND(vfnmsub132sd)
    DI_PROC_REGISTER_COMMAND(vfnmsub213ps)
    DI_PROC_REGISTER_COMMAND(vfnmsub213pd)
    DI_PROC_REGISTER_COMMAND(vfnmsub213ss)
    DI_PROC_REGISTER_COMMAND(vfnmsub213sd)
    DI_PROC_REGISTER_COMMAND(vfnmsub231ps)
    DI_PROC_REGISTER_COMMAND(vfnmsub231pd)
    DI_PROC_REGISTER_COMMAND(vfnmsub231ss)
    DI_PROC_REGISTER_COMMAND(vfnmsub231sd)

    default:
        // the opmask instructions
        Diana_Avx512_RegisterCommands(p);
        break;
    }
}

void DianaProcessor_LinkCommands()
{
    DianaGroupInfo * p = Diana_GetGroups();
    long i = 0, count = Diana_GetGroupsCount();

    for(; i < count; ++i)
    {
        DianaProcessor_OnGroup(p);
        ++p;
    }
}
