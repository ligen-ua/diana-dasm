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
    OPERAND_SIZE temp = 0;
    DI_DEF_LOCALS(src, dest);
    
    DI_MEM_GET_DEST(dest);
    DI_MEM_GET_SRC(src);

    temp = dest;
    DI_START_UPDATE_COA_FLAGS(dest);
    dest = dest + src;
    src = temp;
    DI_CHECK(Di_CheckZeroExtends(pCallContext, &dest, src_size, &dest_size));//first operand
    DI_CHECK(Di_CheckZeroExtends2(pCallContext, &src, dest_size, &src_size));//second operand
    DI_END_UPDATE_COA_FLAGS_ADD(dest, temp);
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

    DI_PROC_REGISTER_COMMAND(fdiv)
    DI_PROC_REGISTER_COMMAND(fdivp)
    DI_PROC_REGISTER_COMMAND(fidiv)
    DI_PROC_REGISTER_COMMAND(fdivr)
    DI_PROC_REGISTER_COMMAND(fdivrp)
    DI_PROC_REGISTER_COMMAND(fidivr)

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

    default:
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
