#ifndef ORTHIA_VMLIB_API_HANDLERS_COMMON_H
#define ORTHIA_VMLIB_API_HANDLERS_COMMON_H

#include "orthia_exec.h"

namespace orthia
{

struct CommonHandlerParameters
{
    const OPERAND_SIZE rip;
    CProcessor * pProcessor;
    IAPIHandlerDebugInterface * pDebugInterface;
    int dianaMode;
    IAddressSpace * pAddressSpace;
    CommonHandlerParameters(OPERAND_SIZE in_rip,
                            CProcessor * in_pProcessor,
                            IAPIHandlerDebugInterface * in_pDebugInterface,
                            int in_dianaMode,
                            IAddressSpace * in_pAddressSpace)
        :
            rip(in_rip),
            pProcessor(in_pProcessor),
            pDebugInterface(in_pDebugInterface),
            dianaMode(in_dianaMode),
            pAddressSpace(in_pAddressSpace)
    {
    }
};
class CCommonAPIHandlerStorage:public RefCountedBase_t<IAPIHandler>
{
private:
    struct Handler
    {
        CommonHandlerFunction_type handler;
        Handler(CommonHandlerFunction_type handler_in)
            :
                handler(handler_in)
        {
        }
    };
    typedef std::map<DI_UINT64, Handler> HandlersMap_type;
    HandlersMap_type m_handlers;

    IAPIHandlerDebugInterface * m_pDebugInterface;
    int m_dianaMode;
    IAddressSpace * m_pAddressSpace;

public:
    CCommonAPIHandlerStorage();
    void Init(IAPIHandlerDebugInterface * pDebugInterface,
              int dianaMode,
              IAddressSpace * pAddressSpace);
    
    virtual IAPIHandlerDebugInterface * GetDebugInterface();

    void RegisterHandler(OPERAND_SIZE rip, CommonHandlerFunction_type pHandler);
    virtual bool HandleAPI(OPERAND_SIZE rip, CProcessor * pProcessor);
};



}

#endif
