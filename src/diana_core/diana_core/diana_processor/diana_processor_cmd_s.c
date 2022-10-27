#include "diana_processor_cmd_s.h"
#include "diana_proc_gen.h"
#include "diana_gen.h"
#include "diana_core_gen_tags.h"

int Diana_Call_sahf(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    pDianaContext;

    pCallContext->m_flags.impl.l.impl.l.impl.l = (DI_CHAR)GET_REG_AH;
    DianaProcessor_SetResetDefaultFlags(pCallContext);
    DI_PROC_END
}

int Diana_Call_setcc(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext,
                     int op)
{
    DI_DEF_LOCAL(dest);
    oldDestValue;
    DI_MEM_GET_DEST(dest);
    dest = op;
    DI_MEM_SET_DEST(dest);
    DI_PROC_END
}

int Diana_Call_seta(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if ((GET_FLAG_CF == 0) && (GET_FLAG_ZF == 0))
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setae(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF == 0)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setb(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setbe(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (GET_FLAG_CF || GET_FLAG_ZF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}


int Diana_Call_sete(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_ZF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setg(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if ((GET_FLAG_SF == GET_FLAG_OF) && (GET_FLAG_ZF == 0))
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setge(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF == GET_FLAG_OF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setl(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF != GET_FLAG_OF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setle(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (GET_FLAG_ZF || (GET_FLAG_SF != GET_FLAG_OF))
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setne(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (!GET_FLAG_ZF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setno(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (!GET_FLAG_OF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setnp(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (!GET_FLAG_PF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setns(struct _dianaContext * pDianaContext,
                     DianaProcessor * pCallContext)
{
    if (!GET_FLAG_SF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_seto(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_OF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_setp(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_PF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}

int Diana_Call_sets(struct _dianaContext * pDianaContext,
                    DianaProcessor * pCallContext)
{
    if (GET_FLAG_SF)
    {
        return Diana_Call_setcc(pDianaContext, 
                                pCallContext,
                                1);
    }
    return Diana_Call_setcc(pDianaContext, 
                            pCallContext,
                            0);
}
