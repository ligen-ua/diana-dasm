 
int DianaProcessor_GetMemValue(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               OPERAND_SIZE size,
                               OPERAND_SIZE * pResult,
                               int flags,
                               DianaUnifiedRegister segReg)
{
    OPERAND_SIZE readed = 0;
    int status = 0;

    flags;

    *pResult = 0;
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

int DianaProcessor_SetMemValue(DianaProcessor * pThis,
                               OPERAND_SIZE selector,
                               OPERAND_SIZE offset,
                               OPERAND_SIZE size,
                               OPERAND_SIZE * pResult,
                               int flags,
                               DianaUnifiedRegister segReg)
{
    OPERAND_SIZE wrote = 0;
    int status = 0;

    flags;

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

void DianaProcessor_SetValueEx(DianaProcessor * pCallContext,
                               DianaUnifiedRegister regId,
                               DianaRegInfo * pReg,
                               OPERAND_SIZE value,
                               int size)
{
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);

    if (regId == reg_RIP || regId == reg_IP)
    {
        pCallContext->m_stateFlags |= DI_PROC_STATE_RIP_CHANGED;
    }

    switch(size)
    {
    case 1:
        *(unsigned char*)pStart = (unsigned char)value;
        UPDATE_PSZ(*(unsigned char*)pStart, 0x80);
        return;
    case 2:
        *(unsigned short*)pStart = (unsigned short)value;
        UPDATE_PSZ(*(unsigned short*)pStart, 0x8000);
        return;
    case 4:
        *(unsigned int*)pStart = (unsigned int)value;
        UPDATE_PSZ(*(unsigned int*)pStart, 0x80000000UL);
        return;
    case 8:
        *(OPERAND_SIZE*)pStart = (OPERAND_SIZE)value;
        UPDATE_PSZ(*(OPERAND_SIZE*)pStart, 0x8000000000000000ULL);
        return;
    }
    Diana_FatalBreak();
}

OPERAND_SIZE DianaProcessor_GetValueEx(DianaProcessor * pCallContext,
                                       DianaRegInfo * pReg,
                                       int size)
{
    char * pStart = (pCallContext->m_pRegistersVector + pReg->m_offset);
    switch(size)
    {
    case 1:
        return *(unsigned char*)pStart;
    case 2:
        return *(unsigned short*)pStart;
    case 4:
        return *(unsigned int*)pStart;
    case 8:
        return *(OPERAND_SIZE*)pStart;
    }
    Diana_FatalBreak();
    return 0;
}

OPERAND_SIZE DianaProcessor_GetValue(DianaProcessor * pCallContext,
                                     DianaRegInfo * pReg)
{
    return DianaProcessor_GetValueEx(pCallContext, pReg, pReg->m_size);
}
