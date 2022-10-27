#include "orthia_plugin_interfaces.h"


namespace orthia
{

// CGenericDebuggerLink
CGenericDebuggerLink::CGenericDebuggerLink(IDebugger * pDebugger)
    :
        m_pDebugger(pDebugger)
{
}
void CGenericDebuggerLink::CheckInterrupt()
{
    if (m_pDebugger->IsInterrupted())
    {
        throw CInterruptException();
    }
}
IDebugger * CGenericDebuggerLink::GetDebuggerInterface()
{
    return m_pDebugger;
}

// CGenericInterruptChecker
CGenericInterruptChecker::CGenericInterruptChecker(IDebugger * pDebugger, int rate)
        :
            m_debugger(pDebugger), m_rate(rate), m_currentIndex(0)
{
    if (rate <= 0)
    {
        throw std::runtime_error("Invalid usage");
    }
}
bool CGenericInterruptChecker::CheckInterrupt()
{
    ++m_currentIndex;
    if (m_currentIndex % m_rate)
    {
        return false;
    }
    m_debugger.CheckInterrupt();
    return true;
}
IDebugger * CGenericInterruptChecker::GetDebuggerInterface()
{
    return m_debugger.GetDebuggerInterface();
}

}