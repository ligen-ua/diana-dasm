#ifndef ORTHIA_VMLIB_API_HANDLERS_WIN32_H
#define ORTHIA_VMLIB_API_HANDLERS_WIN32_H


#include "orthia_exec.h"
#include "orthia_vmlib_api_handlers_common.h"

namespace orthia
{

class CWin32APIHandlerPopulator
{
public:
    CWin32APIHandlerPopulator();
    void RegisterHandlers(ICommonAPIHandlerStorage * pCommonAPIHandlerStorage,
                          IAPIHandlerDebugInterface * pDebugInterface,
                          int dianaMode);
};

}

#endif
