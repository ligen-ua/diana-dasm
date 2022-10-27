#include "diana_processor_cmd_j.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"
#include "diana_processor_cmd_internal.h"


int Diana_Call_ja(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if ((GET_FLAG_CF == 0) && (GET_FLAG_ZF == 0))
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jae(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF == 0)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jb(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jbe(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF || GET_FLAG_ZF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}


int Diana_Call_je(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_ZF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jg(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if ((GET_FLAG_SF == GET_FLAG_OF) && (GET_FLAG_ZF == 0))
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jge(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF == GET_FLAG_OF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jl(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF != GET_FLAG_OF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jle(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (GET_FLAG_ZF || (GET_FLAG_SF != GET_FLAG_OF))
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jne(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (!GET_FLAG_ZF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jno(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (!GET_FLAG_OF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jnp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (!GET_FLAG_PF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jns(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    if (!GET_FLAG_SF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jo(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_OF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jp(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_PF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_js(struct _dianaContext * pDianaContext,
                  DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              1,
                              0);
    }
    DI_PROC_END
}

int Diana_Call_jecxz(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);

    if (!dest)
    {
        return Diana_Call_jcc(pDianaContext, 
                              pCallContext,
                              2,
                              1);
    }
    DI_PROC_END
}

int Diana_Call_jmp(struct _dianaContext * pDianaContext,
                   DianaProcessor * pCallContext)
{
    OPERAND_SIZE newSegment = 0;
    int bNewSegmentIs = 0;

    DI_DEF_LOCAL(newRIP);
    oldDestValue;

    if (pCallContext->m_result.iLinkedOpCount != 1)
    {
        return DI_ERROR;
    }

    // calculate absolute address
    switch(pCallContext->m_result.linkedOperands[0].type)
    {
    case diana_rel:
        newRIP = GET_REG_RIP2(pCallContext->m_context.iCurrentCmd_opsize) + 
                 pCallContext->m_result.linkedOperands[0].value.rel.m_value + 
                 pCallContext->m_result.iFullCmdSize;
        break;

    case diana_index:
    case diana_register:
        DI_MEM_GET_DEST(newRIP);
        break;

    case diana_memory:
        {
            OPERAND_SIZE memSelector = GET_REG_DS, memAddress=0;
            DianaRmIndex  * pIndex = &pCallContext->m_result.linkedOperands[0].value.memory.m_index;
            DI_CHECK(DianaProcessor_CalcIndex(pDianaContext,
                                              pCallContext,
                                              pIndex,
                                              &memSelector,
                                              &memAddress));

            // load new IP and CS
            DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                                memSelector,
                                                memAddress,
                                                sizeof(DI_UINT16), 
                                                &newSegment,
                                                0,
                                                pIndex->seg_reg));

            DI_CHECK(DianaProcessor_GetMemValue(pCallContext, 
                                                memSelector,
                                                memAddress + sizeof(DI_UINT16),
                                                pCallContext->m_result.linkedOperands[0].usedAddressSize,
                                                &newRIP,
                                                0,
                                                pIndex->seg_reg));

            bNewSegmentIs = 1;
            break;
        }
        break;
    case diana_call_ptr:
        newSegment = pCallContext->m_result.linkedOperands[0].value.ptr.m_segment;
        bNewSegmentIs = 1;
        newRIP = pCallContext->m_result.linkedOperands[0].value.ptr.m_address;
        break;
    default:
        return DI_ERROR;
    }


    // set ret IP
    SET_REG_RIP(newRIP);
    if (bNewSegmentIs)
    {
        SET_REG_CS(newSegment);
    }

    DI_PROC_END;
}
