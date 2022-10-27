#include "orthia_vmlib_api_handlers_common.h"

namespace orthia
{

CCommonAPIHandlerStorage::CCommonAPIHandlerStorage()
    :

        m_pDebugInterface(0),
        m_dianaMode(0),
        m_pAddressSpace(0)
{
}

void CCommonAPIHandlerStorage::Init(IAPIHandlerDebugInterface * pDebugInterface,
                                    int dianaMode,
                                    IAddressSpace * pAddressSpace)
{
    m_pDebugInterface = pDebugInterface;
    m_dianaMode = dianaMode;
    m_pAddressSpace = pAddressSpace;
    switch(m_dianaMode)
    {
    default:
        throw std::runtime_error("Invalid mode");
    case DIANA_MODE32:
    case DIANA_MODE64:;
    }
}

IAPIHandlerDebugInterface * CCommonAPIHandlerStorage::GetDebugInterface()
{
    return m_pDebugInterface;
}
bool CCommonAPIHandlerStorage::HandleAPI(OPERAND_SIZE rip, 
                                         CProcessor * pProcessor)
{
    HandlersMap_type::const_iterator it = m_handlers.find(rip);
    if (it == m_handlers.end())
    {
        return false;
    }
    CommonHandlerParameters params(rip, 
                                   pProcessor,
                                   m_pDebugInterface,
                                   m_dianaMode,
                                   m_pAddressSpace);
    return it->second.handler(params);
}

void CCommonAPIHandlerStorage::RegisterHandler(OPERAND_SIZE rip, CommonHandlerFunction_type pHandler)
{
    std::pair<HandlersMap_type::iterator, bool> res = m_handlers.insert(std::make_pair(rip, pHandler));
    if (!res.second)
    {
        throw std::runtime_error("Can't register handler");
    }
}

}

