#include "orthia_vmlib_api_handlers.h"
#include "orthia_vmlib_api_handlers_win32.h"

namespace orthia
{

int SkipStdFunctionCall(CProcessor * pProcessor,
                        const OPERAND_SIZE * pReturn,
                        int operandsCountIncludingReturn,
                        OPERAND_SIZE eaxResult)
{
    DianaProcessor * pCallContext = pProcessor->GetSelf();
    
    OPERAND_SIZE stackSize = (OPERAND_SIZE)(operandsCountIncludingReturn)*pCallContext->m_context.iMainMode_addressSize;
    if (pProcessor->GetSelf()->m_context.iAMD64Mode)
    {
        stackSize = pCallContext->m_context.iMainMode_addressSize;
    }

    OPERAND_SIZE rsp = GET_REG_RSP;
    SET_REG_RSP(rsp + stackSize);

    SET_REG_RAX(eaxResult);
    SET_REG_RIP(*pReturn);
    return DI_SUCCESS;
}

int QueryStdHookArgs(CProcessor * pProcessor, 
                    int argNumber, 
                    OPERAND_SIZE * pValues,
                    int argCount)
{
    if (argCount <= 0)
    {
        return DI_ERROR;
    }
    if (argNumber < 0)
    {
        return DI_ERROR;
    }
    int currentArgCount = 0;

#define REPORT_ARG(X) pValues[currentArgCount] = X; ++argNumber; ++currentArgCount; if (currentArgCount >=argCount) return DI_SUCCESS;

    DianaProcessor * pCallContext = pProcessor->GetSelf();
    if (argNumber == 0)
    {
        OPERAND_SIZE rsp = GET_REG_RSP;
        OPERAND_SIZE value = 0;
        DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                   GET_REG_SS,
                                   rsp,
                                   pCallContext->m_context.iMainMode_addressSize,
                                   &value,
                                   0,
                                   reg_SS));
        REPORT_ARG(value);
    }
    if (pProcessor->GetSelf()->m_context.iAMD64Mode)
    {
        switch(argNumber)
        {
        case 1:
            REPORT_ARG(GET_REG_RCX);
        case 2:
            REPORT_ARG(GET_REG_RDX);
        case 3:
            REPORT_ARG(GET_REG_R8);
        case 4:
            REPORT_ARG(GET_REG_R9);
        }
    }
    for(;;)
    {
        OPERAND_SIZE address = (GET_REG_RSP) + 
                                pCallContext->m_context.iMainMode_addressSize * (argNumber);
        OPERAND_SIZE value = 0;
        DI_CHECK(DianaProcessor_GetMemValue(pCallContext,
                                GET_REG_SS,
                                address,
                                pCallContext->m_context.iMainMode_addressSize,
                                &value,
                                0,
                                reg_SS));
        REPORT_ARG(value);
    }

}
orthia::Ptr<IAPIHandler> CreateAPIHandler(IAPIHandlerDebugInterface * pDebugInterface,
                                          int dianaMode,
                                          IAddressSpace * pAddressSpace)
{
    IAPIHandlerDebugInterface::Debuggee_type type = pDebugInterface->GetDebuggeeType();
    orthia::Ptr<CCommonAPIHandlerStorage> pResult(new CCommonAPIHandlerStorage());
    pResult->Init(pDebugInterface, dianaMode, pAddressSpace);

    switch(type)
    {
    case IAPIHandlerDebugInterface::dtNone:
        break;
    case IAPIHandlerDebugInterface::dtKernel:
        break;
    case IAPIHandlerDebugInterface::dtUser:
        {
            CWin32APIHandlerPopulator populator;
            populator.RegisterHandlers(pResult.get(), pDebugInterface, dianaMode);
        }
        break;
    default:
        throw std::runtime_error("Unknown debuggee type");
    }
    return pResult;
}

}

