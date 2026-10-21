

int DianaProcessor_XMM_GetMemValue(DianaProcessor * pThis,
                                   OPERAND_SIZE selector,
                                   OPERAND_SIZE offset,
                                   OPERAND_SIZE size,
                                   DianaRegisterXMM_type * pResult,
                                   int flags,
                                   DianaUnifiedRegister segReg)
{
    OPERAND_SIZE readed = 0;
    int status = 0;

    flags;

    if (size > sizeof(*pResult))
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    pResult->u64[ 0 ] = 0;
    pResult->u64[ 1 ] = 0;
    status = pThis->m_pMemoryStream->pReadFnc(pThis->m_pMemoryStream, 
                                              selector,
                                              offset, 
                                              pResult, 
                                              size, 
                                              &readed,
                                              pThis,
                                              segReg);
    DI_CHECK(status);

    if (readed != size)
        return DI_ERROR;

    return status;
}

int DianaProcessor_XMM_SetMemValue(DianaProcessor * pThis,
                                   OPERAND_SIZE selector,
                                   OPERAND_SIZE offset,
                                   OPERAND_SIZE size,
                                   DianaRegisterXMM_type * pResult,
                                   int flags,
                                   DianaUnifiedRegister segReg)
{
    OPERAND_SIZE wrote = 0;
    int status = 0;

    flags;

    if (size > sizeof(*pResult))
    {
        Diana_FatalBreak();
        return DI_ERROR;
    }
    status = pThis->m_pMemoryStream->pWriteFnc(pThis->m_pMemoryStream, 
                                               selector,
                                               offset, 
                                               pResult, 
                                               size, 
                                               &wrote,
                                               pThis,
                                               segReg);
    DI_CHECK(status);

    if (wrote != size)
        return DI_ERROR;

    return status;
}

void DianaProcessor_XMM_SetValueEx(DianaProcessor * pCallContext,
                                   DianaUnifiedRegister regId,
                                   DianaRegInfo * pReg,
                                   DianaRegisterXMM_type * value,
                                   int size)
{
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);
    if (size > (int)sizeof(*value))
    {
        // a YMM register: DianaProcessor_YMM_SetValue
        Diana_FatalBreak();
        size = sizeof(*value);
    }
    DIANA_MEMCPY(pStart, value, size);

    regId;
}

DianaRegisterXMM_type DianaProcessor_XMM_GetValueEx(DianaProcessor * pCallContext,
                                                    DianaRegInfo * pReg,
                                                    int size)
{
    DianaRegisterXMM_type r = {0};
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);
    if (size > (int)sizeof(r))
    {
        // a YMM register: DianaProcessor_YMM_GetValue
        Diana_FatalBreak();
        size = sizeof(r);
    }
    DIANA_MEMCPY(r.u8, pStart, size);
    return r;
}

DianaRegisterXMM_type DianaProcessor_XMM_GetValue(DianaProcessor * pCallContext,
                                                  DianaRegInfo * pReg)
{
    return DianaProcessor_XMM_GetValueEx(pCallContext, pReg, pReg->m_size);
}
