#ifndef ORTHIA_VMLIB_API_HANDLERS_H
#define ORTHIA_VMLIB_API_HANDLERS_H

#include "orthia_exec.h"
#include "orthia_vmlib_api_handlers_common.h"

namespace orthia
{

    
int SkipStdFunctionCall(CProcessor * pProcessor,
                        const OPERAND_SIZE * pReturn,
                        int operandsCountIncludingReturn,
                        OPERAND_SIZE eaxResult);

int QueryStdHookArgs(CProcessor * pProcessor, 
                     int argNumber, 
                     OPERAND_SIZE * pValues,
                     int argCount);

orthia::Ptr<IAPIHandler> CreateAPIHandler(IAPIHandlerDebugInterface * pDebugInterface,
                                          int dianaMode,
                                          IAddressSpace * pAddressSpace);


}

#endif
